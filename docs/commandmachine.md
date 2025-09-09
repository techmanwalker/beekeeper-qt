# Command Machine Syntax Specification

## Overview
The Command Machine is a flexible command-line parsing library that handles various option syntaxes while maintaining compatibility with traditional Unix conventions. This document provides a comprehensive specification of its behavior.

## Core Parsing Principles
1. **Left-to-right processing**: Tokens are processed sequentially without backtracking
2. **Value capture**: First value-requiring option captures rest of token
3. **Case sensitivity**: Options are case-sensitive (`-v` ≠ `-V`)
4. **Handler autonomy**: Validation and interpretation delegated to command handlers
5. **Quoting awareness**: Quoted tokens are preserved as single units, including whitespace

## Option Specification
```cpp
struct OptionSpec {
    std::string long_name;     // e.g., "enable-logging"
    std::string short_name;    // e.g., "l" (empty if no short form)
    bool requires_value;       // Does this option require a value?
};
````

## Supported Syntaxes

### 1. Long Options

* `--option` → Flag (value=`"<default>"`)
* `--option=value` → Option with value
* `--option value` → Option with separate value
* `--"option with spaces"` → Long option name may contain spaces if quoted
* `--option="quoted value"` → Value may contain spaces or special characters if quoted

### 2. Short Options

* `-c` → Flag (value=`"<default>"`)
* `-c value` → Option with separate value
* `-c=value` → Option with value (equals syntax)
* `-cvalue` → Option with attached value

### 3. Combined Short Options

* `-abc` → Flags `a`, `b`, `c` (all set to `"<default>"`)
* `-abkvalue` → Flags `a`, `b`; Option `k` with value `"value"`
* `-aKbc` → Flag `a`; Option `K` with value `"bc"`
* `-Vdata` → Option `V` with value `"data"`

### 4. Quoted Values and Subjects

* `"subject with spaces"` → Subject preserved literally
* `--log-level="very verbose"` → Option with quoted value
* `'single quoted value'` → Equivalent to double quotes; preserved as-is
* Quotes are stripped from final values/subjects, but the enclosed text is preserved.

### 5. Special Tokens

* `--` → End of options marker (subsequent tokens are subjects)

## Value Extraction Rules

1. **Value starts immediately** after value-requiring option character
2. **Value includes all subsequent characters** in token (unless quoted separately)
3. **Value may contain special characters or whitespace** when quoted
4. **Examples**:

   * `-aKbc` → `K` value is `"bc"`
   * `-V-c` → `V` value is `"-c"`
   * `-xY=data` → `Y` value is `"=data"`
   * `--option="quoted value"` → `option` value is `"quoted value"`
   * `"uuid with spaces"` → subject `"uuid with spaces"`

## Equals Sign Handling

* **Supported**:

  * `-k=value` (single short option)
  * `--option=value`
  * `--option="quoted value with spaces"`
* **Unsupported**:

  * `-abk=value` (multiple options before `=`)
  * `-a=bc` (when `a` doesn't require value - handler must validate)

## Unsupported Syntaxes

1. **Multiple value options per token**:

   * `-kVdata` → `k` value is `"Vdata"` (only `k` is processed, `V` is not)
2. **Values spanning multiple tokens** (unless quoted):

   * `-k value1 value2` → `value2` becomes subject
3. **Combined tokens with equals**:

   * `-abk=value` → Error
4. **Single hyphen with multiple characters**:

   * `-` → Reserved for stdin
   * `---option` → Invalid

## Duplicate Option Handling

* **Last definition wins**: Final specification takes effect
* **Warning issued**: `"Warning: Option 'X' defined multiple times. Using last definition."`
* **Example**:

  ```bash
  beekeeperman start -v -v -v
  # Outputs 2 warnings
  # Sets v: "<default>"
  ```

## Handler Responsibilities

1. **Validate option types**:

   * Check if flag received value
   * Check if value option missing value
2. **Convert values**:

   * String to boolean, integer, etc.
3. **Interpret `<default>`**:

   * Typically `true` for boolean flags
4. **Handle unknown options**:

   * Parser rejects undefined options

### Example Handler Implementation

```cpp
int handle_start(const std::map<std::string, std::string>& options,
                 const std::vector<std::string>& subjects)
{
    // Boolean flag handling
    if (options.find("verbose") != options.end()) {
        if (options.at("verbose") == "<default>") {
            set_verbose(true);
        } else {
            // Validate explicit value
            if (options.at("verbose") == "true") {
                set_verbose(true);
            } else if (options.at("verbose") == "false") {
                set_verbose(false);
            } else {
                std::cerr << "Error: Invalid --verbose value\n";
                return 1;
            }
        }
    }
    
    // Value option handling
    if (options.find("log-level") != options.end()) {
        try {
            int level = std::stoi(options.at("log-level"));
            set_log_level(level);
        } catch (...) {
            std::cerr << "Error: Invalid --log-level value\n";
            return 1;
        }
    }
    
    // Process subjects
    for (const auto& uuid : subjects) {
        start_beesd(uuid);
    }
    
    return 0;
}
```

## Error Handling

* **Unrecognized option**: `Error: Unrecognized option '-x'`
* **Missing value**: `Error: Option '-k' requires a value`
* **Invalid syntax**: `Error: Equals syntax not supported for combined options`
* **Multiple value options**: Not explicitly checked; first value-requiring option takes the rest of the token (subsequent options in the same token are ignored)
* **Unbalanced quotes**: `Error: Unterminated quoted string`

## Real-World Examples

### Valid Use Cases

```bash
# Start with logging enabled
beekeeperman start --enable-logging 8a3c5f

# Stop with combined flags
beekeeperman stop -vF 8a3c5f

# Complex value example
beekeeperman config -V--option=value 8a3c5f

# Equals syntax
beekeeperman start -c=1024 8a3c5f

# Quoted option value
beekeeperman start --log-level="very verbose" 8a3c5f

# Quoted subject
beekeeperman start "uuid with spaces"
```

### Invalid Use Cases

```bash
# Multiple value options (in same token) - not supported as separate options
beekeeperman start -l3Vdebug 8a3c5f    # Sets l="3Vdebug", V not processed

# Combined with equals
beekeeperman start -abk=value 8a3c5f   # Error: multiple options before =

# Missing value
beekeeperman start -k 8a3c5f           # Error (if next token is subject)

# Unbalanced quotes
beekeeperman start --log-level="high   # Error: Unterminated quoted string
```

## Design Philosophy

1. **Simplicity**: Single-pass token processing
2. **Predictability**: Consistent left-to-right evaluation
3. **Flexibility**: Values and subjects can contain any characters, including spaces (when quoted)
4. **Unix Compatibility**: Traditional syntax support
5. **Separation of Concerns**: Parser parses, handler validates

## Frequently Asked Questions

### Why not support multiple value options per token?

This would introduce ambiguity in value assignment and require complex lookahead logic, violating our simplicity principle.

### Why allow values with special characters or spaces?

Many applications need to accept values containing `-`, `=`, spaces, or other special characters. Quoting provides maximum flexibility while maintaining unambiguous parsing.

### How should handlers interpret `<default>`?

For boolean flags: treat as `true`. For other options: use application-specific default or error if value is required.

### Why case-sensitive options?

This follows Unix conventions where `-v` and `-V` often mean different things (e.g., `grep -v` vs `grep -V`).

## License

This project is licensed under the [AGPLv3](https://www.gnu.org/licenses/agpl-3.0.txt).