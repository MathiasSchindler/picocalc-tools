BEGIN {
    status = 0
}

FNR == NR {
    if ($1 ~ /^EMU_HASH_[A-Z_]+$/ && $2 == ":=") {
        hashes[$1] = $3
    }
    next
}

FNR == 1 {
    next
}

{
    split($0, fields, "\t")
    name = fields[1]
    hash_var = fields[7]
    expected = fields[8]
    if (!(hash_var in hashes)) {
        printf("missing %s for replay %s\n", hash_var, name) > "/dev/stderr"
        status = 1
        next
    }
    if (hashes[hash_var] != expected) {
        printf("hash mismatch for replay %s: manifest %s=%s, Makefile has %s\n", name, hash_var, expected, hashes[hash_var]) > "/dev/stderr"
        status = 1
    }
}

END {
    exit status
}