#ifndef OSC_PARSER 
#define OSC_PARSER 

// Lightweight OSC 1.0 parser for ESP32.
// Usage:
//   osc_parser osc;
//   if (osc.parse(data, len) && osc.match("/led/brightness"))
//       float v = osc.read<float>(0);


#include <cstdint>
#include <cstring>
#include <type_traits>
#include <Arduino.h> 

struct osc_parser
{
    // ── Parse result codes ────────────────────────────────────────────────────
    enum class status : uint8_t
    {
        ok,
        bad_alignment,   // packet length not a multiple of 4
        no_address,      // does not start with '/'
        no_type_tag,     // type tag string missing or malformed
        out_of_range,    // read() index exceeds argument count
        type_mismatch,   // read<T>() called with wrong T for that slot
        not_parsed,      // read/match called before a successful parse
    };

    osc_parser() {reset(); }

    // ── Main entry point ──────────────────────────────────────────────────────
    // Parses a raw OSC packet. Returns true on success.
    // 'data' must remain valid for the lifetime of subsequent match()/read() calls
    // (we store pointers into it rather than copying).
    bool parse(const char* data, uint16_t len) noexcept
    {
        reset();

        if (!data || len == 0)                  { last_status = status::no_address;   return false; }
        if (len % 4 != 0)                       { last_status = status::bad_alignment; return false; }
        if (data[0] != '/')                     { last_status = status::no_address;   return false; }

        // ── Address ───────────────────────────────────────────────────────────
        address = data;
        uint16_t cursor = aligned_end(data, 0, len);
        if (cursor >= len)                      { last_status = status::no_type_tag;  return false; }

        // ── Type tag string ───────────────────────────────────────────────────
        if (data[cursor] != ',')                { last_status = status::no_type_tag;  return false; }
        const char* tags_raw = data + cursor + 1;   // skip the leading ','
        uint16_t tag_count = 0;
        while (tags_raw[tag_count] != '\0' && tag_count < max_args)
            tag_count++;

        if (tag_count > max_args)               { last_status = status::no_type_tag;  return false; }
        cursor = aligned_end(data, cursor, len);
        if (cursor > len)                       { last_status = status::no_type_tag;  return false; }

        // ── Arguments ─────────────────────────────────────────────────────────
        arg_count = 0;
        for (uint16_t i = 0; i < tag_count && cursor + 4 <= len; ++i)
        {
            args[arg_count].tag  = tags_raw[i];
            args[arg_count].data = data + cursor;
            cursor += 4;    // every basic OSC type (i, f, T, F) is 4 bytes
            arg_count++;
        }

        last_status = status::ok;
        return true;
    }

    // ── Address matching ──────────────────────────────────────────────────────
    [[nodiscard]] bool match(const char* pattern) const noexcept
    {
        if (last_status != status::ok || !address) return false;
        return strncmp(address, pattern, max_address_len) == 0;
    }

    // ── Typed argument read ───────────────────────────────────────────────────
    // T must be int32_t or float.
    // index is 0-based.
    // Returns 0 / 0.0f on error; check last_status for details.
    template<typename T>
    [[nodiscard]] T read(uint8_t index = 0) noexcept
    {
        static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, float>,
                      "osc_parser::read<T>: T must be int32_t or float");

        if (last_status != status::ok)          { last_status = status::not_parsed;   return T{}; }
        if (index >= arg_count)                 { last_status = status::out_of_range; return T{}; }

        const arg_slot& slot = args[index];
        const char expected_tag = std::is_same_v<T, float> ? 'f' : 'i';
        if (slot.tag != expected_tag)           { last_status = status::type_mismatch; return T{}; }

        // OSC is big-endian; ESP32 is little-endian — byte-swap required.
        T value;
        const auto* src = reinterpret_cast<const uint8_t*>(slot.data);
        auto*       dst = reinterpret_cast<uint8_t*>(&value);
        dst[0] = src[3];
        dst[1] = src[2];
        dst[2] = src[1];
        dst[3] = src[0];
        return value;
    }

    // ── Diagnostics ───────────────────────────────────────────────────────────
    [[nodiscard]] status       get_status()    const noexcept { return last_status; }
    [[nodiscard]] bool         ok()            const noexcept { return last_status == status::ok; }
    [[nodiscard]] const char*  get_address()   const noexcept { return address ? address : ""; }
    [[nodiscard]] uint8_t      get_arg_count() const noexcept { return arg_count; }
    [[nodiscard]] char         get_type_tag(uint8_t index) const noexcept
    {
        return index < arg_count ? args[index].tag : '\0';
    }

    void dump() const noexcept
    {
        Serial.printf("[osc_parser] status=%d address=%s args=%u\n",
                      static_cast<int>(last_status),
                      address ? address : "(null)",
                      arg_count);
        for (uint8_t i = 0; i < arg_count; ++i)
        {
            if      (args[i].tag == 'f') Serial.printf("  [%u] float : %f\n",  i, const_cast<osc_parser*>(this)->read<float>(i));
            else if (args[i].tag == 'i') Serial.printf("  [%u] int   : %ld\n", i, (long)const_cast<osc_parser*>(this)->read<int32_t>(i));
            else                         Serial.printf("  [%u] tag '%c' (unsupported)\n", i, args[i].tag);
        }
    }

private:
    static constexpr uint8_t  max_args        = 8;
    static constexpr uint16_t max_address_len = 128;

    struct arg_slot
    {
        const char* data = nullptr;   // pointer into the original packet
        char        tag  = '\0';
    };

    const char* address   = nullptr;
    arg_slot    args[max_args]{};
    uint8_t     arg_count = 0;
    status      last_status = status::not_parsed;

    void reset() noexcept
    {
        address     = nullptr;
        arg_count   = 0;
        last_status = status::not_parsed;
        for (auto& a : args) a = {};
    }

    // Returns the cursor position just after the null-terminated, 4-byte-aligned
    // field that starts at 'start' within 'data'.
    static uint16_t aligned_end(const char* data, uint16_t start, uint16_t len) noexcept
    {
        uint16_t i = start;
        while (i < len && data[i] != '\0') ++i;     // find null terminator
        i++;                                          // step past it
        return (i + 3) & ~uint16_t{3};               // round up to next multiple of 4
    }
};

#endif OSC_PARSER>
