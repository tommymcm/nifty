# Doxygen Search CLI - Project Plan

## Overview
A TypeScript CLI application for searching and querying Doxygen XML output. Built with Bun for fast builds and package management.

## Project Goals
- Provide fast, intuitive search for classes and functions in Doxygen-generated documentation
- Build a solid foundation that can scale to support fuzzy search, AI assistance, and TUI interfaces
- Maintain clean separation of concerns for easy testing and extension

## Technology Stack
- **Runtime/Build**: Bun
- **Language**: TypeScript
- **CLI Framework**: commander
- **XML Parsing**: fast-xml-parser
- **Testing**: Bun's built-in test runner

## Project Structure
```
doxygen-search/
├── src/
│   ├── cli/           # CLI interface and command handling
│   ├── parser/        # Doxygen XML parsing logic
│   ├── search/        # Search engine implementations
│   └── index.ts       # Main entry point
├── tests/             # Test files
├── examples/          # Sample Doxygen XML for testing
├── package.json
├── tsconfig.json
└── PLAN.md           # This file
```

## Development Phases

### Phase 1: Core Functionality (Current)
**Goal**: Working CLI that can search Doxygen XML for classes and functions

**Deliverables**:
- XML parser that extracts class and function definitions
- Simple string-based search engine
- CLI with basic commands (class, function search)
- Text-based output formatting
- Basic error handling and validation

**Success Criteria**:
- Can parse standard Doxygen XML output
- Can search by class name and function name
- Returns accurate results with file location and line numbers
- Handles missing files and invalid XML gracefully

### Phase 2: Enhanced Search (Future)
**Goal**: Improve search capabilities and user experience

**Features**:
- Fuzzy search using `fuse.js`
- Search result caching for performance
- Multiple output formats (JSON, table)
- Configuration file support
- Advanced filtering (by namespace, file, etc.)

### Phase 3: Advanced Features (Future)
**Goal**: AI and visual enhancements

**Features**:
- AI-assisted semantic search
- TUI interface with `ink` or `blessed`
- Interactive mode with result navigation
- Integration with code editors

## Architecture Principles

### 1. Separation of Concerns
- **CLI layer**: Only handles user interaction and command parsing
- **Parser layer**: Only responsible for XML → data model conversion
- **Search layer**: Pure search logic, no I/O dependencies
- **Formatter layer**: Output generation independent of search logic

### 2. Pluggable Design
- Search engines implement common interface
- Output formatters are swappable
- Easy to add new entity types beyond classes/functions

### 3. Testability
- All core logic is pure functions where possible
- File I/O abstracted for mocking
- Each layer independently testable

### 4. Performance
- Lazy parsing: only load what's needed
- Result caching for repeated queries
- Efficient data structures for large codebases

## Configuration Strategy

### CLI Arguments (Priority 1)
```bash
--path <dir>        # Path to Doxygen XML output
--format <type>     # Output format (text, json, table)
--case-sensitive    # Case-sensitive search
--verbose          # Detailed output
```

### Config File (Priority 2)
`.doxysearch.json` in project root or home directory:
```json
{
  "doxygenPath": "./docs/xml",
  "outputFormat": "text",
  "caseSensitive": false
}
```

## Error Handling Strategy
- Validate Doxygen XML path exists before parsing
- Graceful degradation for malformed XML
- Clear error messages with suggestions
- Non-zero exit codes for scripting

## Testing Strategy
- Unit tests for parser, search, and formatters
- Integration tests for CLI commands
- Test fixtures with sample Doxygen XML
- Performance benchmarks for large codebases

## Documentation Plan
- README.md with installation and usage examples
- API documentation for extending the tool
- Examples directory with sample queries
- Contributing guidelines

## Release Strategy
1. Local development and testing
2. npm package publication
3. Optional: Binary distribution with `bun build --compile`

## Success Metrics
- Can handle Doxygen output from real-world C++ projects
- Search results returned in < 100ms for codebases up to 10k entities
- Zero dependencies on external tools (besides Bun)
- Clean, intuitive CLI that follows Unix conventions

## Future Considerations
- Language server protocol (LSP) integration
- Web interface for documentation browsing
- Export to other formats (Markdown, HTML)
- Integration with CI/CD for documentation validation