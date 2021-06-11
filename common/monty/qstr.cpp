// qstr.cpp - set of "quick strings" from MicroPython 1.14 plus own entries

#include "monty.h"

using namespace monty;

//CG2 mod-list d
///extern Module ext_machine;
extern Module ext_sys;

Module ext_sys (Q (174,"sys")); // FIXME hack to satisfy linker

static Lookup::Item const mod_map [] = {
//CG< mod-list a
    { Q (174,"sys"), ext_sys },
#if NATIVE
    ///{ Q (207,"machine"), ext_machine },
#endif
#if STM32
    ///{ Q (218,"machine"), ext_machine },
#endif
//CG>
};

static Lookup const mod_attrs (mod_map);
Dict Module::loaded (&mod_attrs);

extern char const monty::qstrBase [] =
//CG< qstr-emit
#if NATIVE
    "\xA2\x01\x71\x02\x72\x02\x7A\x02\x7C\x02\x7E\x02\x80\x02\x82\x02"
    "\x8B\x02\x8D\x02\x96\x02\xA0\x02\xAC\x02\xB6\x02\xBF\x02\xCB\x02"
    "\xD7\x02\xE0\x02\xE9\x02\xF1\x02\xFA\x02\x02\x03\x0B\x03\x16\x03"
    "\x1F\x03\x27\x03\x30\x03\x3D\x03\x46\x03\x52\x03\x5A\x03\x6A\x03"
    "\x79\x03\x88\x03\x96\x03\x9F\x03\xA8\x03\xB2\x03\xC0\x03\xCC\x03"
    "\xDD\x03\xE8\x03\xF1\x03\x03\x04\x0F\x04\x1B\x04\x25\x04\x2E\x04"
    "\x42\x04\x4A\x04\x58\x04\x65\x04\x73\x04\x7F\x04\x8A\x04\x94\x04"
    "\x9F\x04\xB1\x04\xB5\x04\xB9\x04\xBD\x04\xC4\x04\xC9\x04\xCE\x04"
    "\xD7\x04\xE1\x04\xEA\x04\xF0\x04\xF9\x04\xFD\x04\x09\x05\x0F\x05"
    "\x15\x05\x1B\x05\x20\x05\x26\x05\x2B\x05\x2F\x05\x36\x05\x3A\x05"
    "\x43\x05\x48\x05\x4D\x05\x54\x05\x59\x05\x60\x05\x6B\x05\x6F\x05"
    "\x77\x05\x7F\x05\x87\x05\x8C\x05\x8F\x05\x95\x05\x9C\x05\xA0\x05"
    "\xA8\x05\xB0\x05\xBB\x05\xC3\x05\xCB\x05\xD6\x05\xDE\x05\xE4\x05"
    "\xE9\x05\xEE\x05\xF2\x05\xF7\x05\xFB\x05\x00\x06\x07\x06\x0E\x06"
    "\x14\x06\x1B\x06\x20\x06\x24\x06\x30\x06\x35\x06\x3C\x06\x41\x06"
    "\x45\x06\x49\x06\x51\x06\x55\x06\x5B\x06\x61\x06\x66\x06\x6F\x06"
    "\x78\x06\x7F\x06\x87\x06\x8C\x06\x94\x06\x9A\x06\xA1\x06\xA7\x06"
    "\xAE\x06\xB5\x06\xBA\x06\xBF\x06\xC3\x06\xC7\x06\xCF\x06\xDA\x06"
    "\xDF\x06\xE6\x06\xEC\x06\xF2\x06\xFD\x06\x0A\x07\x0F\x07\x14\x07"
    "\x18\x07\x1E\x07\x22\x07\x28\x07\x2E\x07\x37\x07\x3D\x07\x42\x07"
    "\x49\x07\x4F\x07\x55\x07\x5B\x07\x62\x07\x68\x07\x6C\x07\x75\x07"
    "\x82\x07\x8D\x07\x98\x07\xA2\x07\xA5\x07\xAB\x07\xB3\x07\xB7\x07"
    "\xBD\x07\xC5\x07\xD4\x07\xDA\x07\xE2\x07\xED\x07\xF8\x07\xFF\x07"
    "\x0B\x08\x15\x08\x1C\x08\x20\x08\x24\x08\x28\x08\x2D\x08\x33\x08"
    "\x39\x08\x3F\x08\x45\x08\x4B\x08\x53\x08\x5C\x08\x66\x08\x72\x08"
    "\x7D\x08\x88\x08\x91\x08\x9A\x08\xA1\x08\xAC\x08\xB3\x08\xB9\x08"
    "\xC1\x08"
    // offsets [0..417], hashes [418..624], 207 strings [625..2240]
#endif
#if STM32
    "\xC0\x01\x9E\x02\x9F\x02\xA7\x02\xA9\x02\xAB\x02\xAD\x02\xAF\x02"
    "\xB8\x02\xBA\x02\xC3\x02\xCD\x02\xD9\x02\xE3\x02\xEC\x02\xF8\x02"
    "\x04\x03\x0D\x03\x16\x03\x1E\x03\x27\x03\x2F\x03\x38\x03\x43\x03"
    "\x4C\x03\x54\x03\x5D\x03\x6A\x03\x73\x03\x7F\x03\x87\x03\x97\x03"
    "\xA6\x03\xB5\x03\xC3\x03\xCC\x03\xD5\x03\xDF\x03\xED\x03\xF9\x03"
    "\x0A\x04\x15\x04\x1E\x04\x30\x04\x3C\x04\x48\x04\x52\x04\x5B\x04"
    "\x6F\x04\x77\x04\x85\x04\x92\x04\xA0\x04\xAC\x04\xB7\x04\xC1\x04"
    "\xCC\x04\xDE\x04\xE2\x04\xE6\x04\xEA\x04\xF1\x04\xF6\x04\xFB\x04"
    "\x04\x05\x0E\x05\x17\x05\x1D\x05\x26\x05\x2A\x05\x36\x05\x3C\x05"
    "\x42\x05\x48\x05\x4D\x05\x53\x05\x58\x05\x5C\x05\x63\x05\x67\x05"
    "\x70\x05\x75\x05\x7A\x05\x81\x05\x86\x05\x8D\x05\x98\x05\x9C\x05"
    "\xA4\x05\xAC\x05\xB4\x05\xB9\x05\xBC\x05\xC2\x05\xC9\x05\xCD\x05"
    "\xD5\x05\xDD\x05\xE8\x05\xF0\x05\xF8\x05\x03\x06\x0B\x06\x11\x06"
    "\x16\x06\x1B\x06\x1F\x06\x24\x06\x28\x06\x2D\x06\x34\x06\x3B\x06"
    "\x41\x06\x48\x06\x4D\x06\x51\x06\x5D\x06\x62\x06\x69\x06\x6E\x06"
    "\x72\x06\x76\x06\x7E\x06\x82\x06\x88\x06\x8E\x06\x93\x06\x9C\x06"
    "\xA5\x06\xAC\x06\xB4\x06\xB9\x06\xC1\x06\xC7\x06\xCE\x06\xD4\x06"
    "\xDB\x06\xE2\x06\xE7\x06\xEC\x06\xF0\x06\xF4\x06\xFC\x06\x07\x07"
    "\x0C\x07\x13\x07\x19\x07\x1F\x07\x2A\x07\x37\x07\x3C\x07\x41\x07"
    "\x45\x07\x4B\x07\x4F\x07\x55\x07\x5B\x07\x64\x07\x6A\x07\x6F\x07"
    "\x76\x07\x7C\x07\x82\x07\x88\x07\x8F\x07\x95\x07\x99\x07\xA2\x07"
    "\xAF\x07\xBA\x07\xC5\x07\xCF\x07\xD2\x07\xD8\x07\xE0\x07\xE4\x07"
    "\xEA\x07\xF2\x07\x01\x08\x07\x08\x0F\x08\x1A\x08\x25\x08\x2C\x08"
    "\x38\x08\x42\x08\x49\x08\x4D\x08\x51\x08\x55\x08\x5A\x08\x60\x08"
    "\x66\x08\x6C\x08\x72\x08\x78\x08\x80\x08\x89\x08\x93\x08\x9F\x08"
    "\xAA\x08\xB5\x08\xBE\x08\xC7\x08\xCE\x08\xD9\x08\xE1\x08\xE8\x08"
    "\xED\x08\xF2\x08\xF8\x08\xFD\x08\x04\x09\x08\x09\x0D\x09\x12\x09"
    "\x16\x09\x1D\x09\x23\x09\x2B\x09\x32\x09\x38\x09\x3F\x09\x44\x09"
    // offsets [0..447], hashes [448..669], 222 strings [670..2371]
#endif
    "\x05\x7A\xB0\x85\x8F\x8A\xBD\xFA\xA7\x2B\xFD\x6D\x45\x40\x26\xF7"
    "\x5F\x16\xCF\xE2\x8E\xFF\xE2\x79\x02\x6B\x10\x32\xD0\x2D\x97\x21"
    "\x07\x91\xF0\xF2\x16\x20\x5C\x83\xEA\xAF\xFF\xDC\xBA\x17\xC6\xA1"
    "\x81\x61\xEA\x94\x20\x25\x96\xB6\x95\x44\x13\x6B\xC2\xEB\xF7\x76"
    "\x22\x5C\x0D\xDC\xB4\x7C\x33\xC0\xE0\xA6\x3F\xFA\xB8\x0A\x1B\x9B"
    "\x1E\x63\x00\x26\x35\x33\xC0\x9D\x8C\xB7\x28\x7B\x12\x16\xEB\xA8"
    "\xB6\xFC\x5B\xB5\xDD\xE3\x8F\xA7\x32\x01\x62\x27\x89\x3B\xC6\xE5"
    "\xCE\xB9\x0B\x42\x90\xD1\x1C\x2A\xBF\x2D\x54\x1A\xB7\x4B\xF9\x63"
    "\x49\xD0\x25\xD2\xE9\xE7\xA5\x3B\x79\xB9\x23\x27\xD4\x6C\xBF\x5E"
    "\xB7\x85\x74\x62\x57\x9D\x50\x29\x2E\xC4\xB3\xD8\xFD\x9D\xB4\x27"
    "\xB7\x4E\x7D\x98\xE6\xB2\x22\x2E\xB0\x03\x61\xD5\xE0\xBC\xEE\xEC"
    "\x17\x64\xBF\x40\x8F\xE1\xC1\x56\x15\xA3\xF4\xFC\x8E\xA4\x7C\xAB"
    "\xC9\xB5\xA7\xA7\x98\xB0\x25\x05\x45\xF8\x4D\xF4"
#if NATIVE
    "\x87\x43\x60"
#endif
#if STM32
    "\x91\x04\xAC\xE7\xEA\xED\xA6\x29\x4F\x9E\xCF\x87\x43\x60\x43\x0D"
    "\x7C\x41"
#endif
    // end of 1-byte hashes, start of string data:
    ""                     "\0" // 1
    "__dir__"              "\0" // 2
    "\x0a"                 "\0" // 3
    " "                    "\0" // 4
    "*"                    "\0" // 5
    "/"                    "\0" // 6
    "<module>"             "\0" // 7
    "_"                    "\0" // 8
    "__call__"             "\0" // 9
    "__class__"            "\0" // 10
    "__delitem__"          "\0" // 11
    "__enter__"            "\0" // 12
    "__exit__"             "\0" // 13
    "__getattr__"          "\0" // 14
    "__getitem__"          "\0" // 15
    "__hash__"             "\0" // 16
    "__init__"             "\0" // 17
    "__int__"              "\0" // 18
    "__iter__"             "\0" // 19
    "__len__"              "\0" // 20
    "__main__"             "\0" // 21
    "__module__"           "\0" // 22
    "__name__"             "\0" // 23
    "__new__"              "\0" // 24
    "__next__"             "\0" // 25
    "__qualname__"         "\0" // 26
    "__repr__"             "\0" // 27
    "__setitem__"          "\0" // 28
    "__str__"              "\0" // 29
    "ArithmeticError"      "\0" // 30
    "AssertionError"       "\0" // 31
    "AttributeError"       "\0" // 32
    "BaseException"        "\0" // 33
    "EOFError"             "\0" // 34
    "Ellipsis"             "\0" // 35
    "Exception"            "\0" // 36
    "GeneratorExit"        "\0" // 37
    "ImportError"          "\0" // 38
    "IndentationError"     "\0" // 39
    "IndexError"           "\0" // 40
    "KeyError"             "\0" // 41
    "KeyboardInterrupt"    "\0" // 42
    "LookupError"          "\0" // 43
    "MemoryError"          "\0" // 44
    "NameError"            "\0" // 45
    "NoneType"             "\0" // 46
    "NotImplementedError"  "\0" // 47
    "OSError"              "\0" // 48
    "OverflowError"        "\0" // 49
    "RuntimeError"         "\0" // 50
    "StopIteration"        "\0" // 51
    "SyntaxError"          "\0" // 52
    "SystemExit"           "\0" // 53
    "TypeError"            "\0" // 54
    "ValueError"           "\0" // 55
    "ZeroDivisionError"    "\0" // 56
    "abs"                  "\0" // 57
    "all"                  "\0" // 58
    "any"                  "\0" // 59
    "append"               "\0" // 60
    "args"                 "\0" // 61
    "bool"                 "\0" // 62
    "builtins"             "\0" // 63
    "bytearray"            "\0" // 64
    "bytecode"             "\0" // 65
    "bytes"                "\0" // 66
    "callable"             "\0" // 67
    "chr"                  "\0" // 68
    "classmethod"          "\0" // 69
    "clear"                "\0" // 70
    "close"                "\0" // 71
    "const"                "\0" // 72
    "copy"                 "\0" // 73
    "count"                "\0" // 74
    "dict"                 "\0" // 75
    "dir"                  "\0" // 76
    "divmod"               "\0" // 77
    "end"                  "\0" // 78
    "endswith"             "\0" // 79
    "eval"                 "\0" // 80
    "exec"                 "\0" // 81
    "extend"               "\0" // 82
    "find"                 "\0" // 83
    "format"               "\0" // 84
    "from_bytes"           "\0" // 85
    "get"                  "\0" // 86
    "getattr"              "\0" // 87
    "globals"              "\0" // 88
    "hasattr"              "\0" // 89
    "hash"                 "\0" // 90
    "id"                   "\0" // 91
    "index"                "\0" // 92
    "insert"               "\0" // 93
    "int"                  "\0" // 94
    "isalpha"              "\0" // 95
    "isdigit"              "\0" // 96
    "isinstance"           "\0" // 97
    "islower"              "\0" // 98
    "isspace"              "\0" // 99
    "issubclass"           "\0" // 100
    "isupper"              "\0" // 101
    "items"                "\0" // 102
    "iter"                 "\0" // 103
    "join"                 "\0" // 104
    "key"                  "\0" // 105
    "keys"                 "\0" // 106
    "len"                  "\0" // 107
    "list"                 "\0" // 108
    "little"               "\0" // 109
    "locals"               "\0" // 110
    "lower"                "\0" // 111
    "lstrip"               "\0" // 112
    "main"                 "\0" // 113
    "map"                  "\0" // 114
    "micropython"          "\0" // 115
    "next"                 "\0" // 116
    "object"               "\0" // 117
    "open"                 "\0" // 118
    "ord"                  "\0" // 119
    "pop"                  "\0" // 120
    "popitem"              "\0" // 121
    "pow"                  "\0" // 122
    "print"                "\0" // 123
    "range"                "\0" // 124
    "read"                 "\0" // 125
    "readinto"             "\0" // 126
    "readline"             "\0" // 127
    "remove"               "\0" // 128
    "replace"              "\0" // 129
    "repr"                 "\0" // 130
    "reverse"              "\0" // 131
    "rfind"                "\0" // 132
    "rindex"               "\0" // 133
    "round"                "\0" // 134
    "rsplit"               "\0" // 135
    "rstrip"               "\0" // 136
    "self"                 "\0" // 137
    "send"                 "\0" // 138
    "sep"                  "\0" // 139
    "set"                  "\0" // 140
    "setattr"              "\0" // 141
    "setdefault"           "\0" // 142
    "sort"                 "\0" // 143
    "sorted"               "\0" // 144
    "split"                "\0" // 145
    "start"                "\0" // 146
    "startswith"           "\0" // 147
    "staticmethod"         "\0" // 148
    "step"                 "\0" // 149
    "stop"                 "\0" // 150
    "str"                  "\0" // 151
    "strip"                "\0" // 152
    "sum"                  "\0" // 153
    "super"                "\0" // 154
    "throw"                "\0" // 155
    "to_bytes"             "\0" // 156
    "tuple"                "\0" // 157
    "type"                 "\0" // 158
    "update"               "\0" // 159
    "upper"                "\0" // 160
    "utf-8"                "\0" // 161
    "value"                "\0" // 162
    "values"               "\0" // 163
    "write"                "\0" // 164
    "zip"                  "\0" // 165
    "<object>"             "\0" // 166
    "UnicodeError"         "\0" // 167
    "<instance>"           "\0" // 168
    "<dictview>"           "\0" // 169
    "__bases__"            "\0" // 170
    "gc"                   "\0" // 171
    "gcmax"                "\0" // 172
    "gcstats"              "\0" // 173
    "sys"                  "\0" // 174
    "ready"                "\0" // 175
    "modules"              "\0" // 176
    "implementation"       "\0" // 177
    "monty"                "\0" // 178
    "version"              "\0" // 179
    "<bytecode>"           "\0" // 180
    "<callable>"           "\0" // 181
    "<cell>"               "\0" // 182
    "<boundmeth>"          "\0" // 183
    "<closure>"            "\0" // 184
    "<pyvm>"               "\0" // 185
    "foo"                  "\0" // 186
    "bar"                  "\0" // 187
    "baz"                  "\0" // 188
    "wait"                 "\0" // 189
    "trace"                "\0" // 190
    "array"                "\0" // 191
    "class"                "\0" // 192
    "event"                "\0" // 193
    "slice"                "\0" // 194
    "argtest"              "\0" // 195
    "<buffer>"             "\0" // 196
    "<context>"            "\0" // 197
    "<exception>"          "\0" // 198
    "<function>"           "\0" // 199
    "<iterator>"           "\0" // 200
    "<lookup>"             "\0" // 201
    "<method>"             "\0" // 202
    "<none>"               "\0" // 203
    "<stacklet>"           "\0" // 204
#if NATIVE
    "ticker"               "\0" // 205
    "ticks"                "\0" // 206
    "machine"              "\0" // 207
#endif
#if STM32
    "disable"              "\0" // 205
    "enable"               "\0" // 206
    "xfer"                 "\0" // 207
    "recv"                 "\0" // 208
    "sleep"                "\0" // 209
    "xmit"                 "\0" // 210
    "cycles"               "\0" // 211
    "dog"                  "\0" // 212
    "kick"                 "\0" // 213
    "rf69"                 "\0" // 214
    "spi"                  "\0" // 215
    "ticker"               "\0" // 216
    "ticks"                "\0" // 217
    "machine"              "\0" // 218
    "<pins>"               "\0" // 219
    "<spi>"                "\0" // 220
    "<rf69>"               "\0" // 221
    "pins"                 "\0" // 222
#endif
//CG>
;

int const monty::qstrBaseLen = sizeof qstrBase;
