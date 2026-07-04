BEGIN {
    status = 0
    app_base = hex_to_dec("0x10032000")
    ram_base = hex_to_dec("0x20000000")
    ram_end = hex_to_dec("0x20040000")
    stack_top = hex_to_dec("0x20042000")
}

function hex_to_dec(text,    value, pos, digit, chars) {
    value = 0
    chars = "0123456789abcdef"
    if (text ~ /^0x/) text = substr(text, 3)
    for (pos = 1; pos <= length(text); pos += 1) {
        digit = index(chars, substr(tolower(text), pos, 1)) - 1
        if (digit < 0) return -1
        value = value * 16 + digit
    }
    return value
}

function fail(message) {
    printf("%s: %s\n", FILENAME, message) > "/dev/stderr"
    status = 1
}

function require_symbol(name) {
    if (count[name] != 1) fail(name " missing or duplicated")
}

function require_eq(name, expected) {
    require_symbol(name)
    if (value[name] != expected) fail(name " has unexpected value")
}

$1 ~ /^0x[0-9a-fA-F]+$/ && $2 ~ /^__/ {
    count[$2] += 1
    value[$2] = hex_to_dec($1)
}

END {
    require_eq("__text_start", app_base)
    require_symbol("__text_end")
    require_symbol("__data_source")
    require_symbol("__data_start")
    require_symbol("__data_end")
    require_symbol("__bss_start")
    require_symbol("__bss_end")
    require_eq("__StackTop", stack_top)
    require_eq("__stack", stack_top)

    if (value["__text_end"] < value["__text_start"]) fail("__text_end before __text_start")
    if (value["__data_source"] < value["__text_end"]) fail("__data_source before __text_end")
    if (value["__data_start"] < ram_base || value["__data_start"] > ram_end) fail("__data_start outside RAM")
    if (value["__data_end"] < value["__data_start"] || value["__data_end"] > ram_end) fail("__data_end outside RAM or before start")
    if (value["__bss_start"] < value["__data_end"] || value["__bss_start"] > ram_end) fail("__bss_start outside RAM or before data end")
    if (value["__bss_end"] < value["__bss_start"] || value["__bss_end"] > ram_end) fail("__bss_end outside RAM or before bss start")
    if (value["__data_source"] - app_base > image_size) fail("__data_source outside image")
    if (value["__text_end"] - app_base > image_size) fail("__text_end outside image")
    exit status
}