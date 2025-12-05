# CLI Directory Plan

## Purpose
Handles all user interaction, command-line argument parsing, and orchestration of the search workflow. This layer should be thin, delegating actual work to parser and search modules.

## Structure
```
cli/
├── index.ts           # CLI entry point, sets up commander
├── commands/
│   ├── search.ts      # Search command implementation
│   ├── list.ts        # List entities command (future)
│   ├── info.ts        # Show detailed entity info (future)
│   └── index.ts       # Command exports
├── config.ts          # Configuration loading and validation
├── output.ts          # Output helpers (colors, formatting)
└── validators.ts      # Input validation functions
```

## Responsibilities

### 1. Application Entry Point (`index.ts`)

Sets up the CLI application and orchestrates command execution.

```typescript
import { Command } from 'commander';
import { searchCommand } from './commands';
import { loadConfig } from './config';

export async function main(argv: string[]): Promise<number> {
  const program = new Command();
  
  program
    .name('doxygen-search')
    .description('Search and query Doxygen XML documentation')
    .version('0.1.0')
    .option('-p, --path <dir>', 'Path to Doxygen XML output directory')
    .option('-f, --format <type>', 'Output format (text|json|table)', 'text')
    .option('-v, --verbose', 'Enable verbose output')
    .option('--no-color', 'Disable colored output')
    .option('--config <file>', 'Path to config file');

  // Register commands
  program
    .command('search')
    .description('Search for classes, functions, and other entities')
    .argument('<query>', 'Search query')
    .option('-t, --type <kind>', 'Filter by entity type (class|function|all)', 'all')
    .option('-e, --exact', 'Exact match only')
    .option('-i, --case-insensitive', 'Case-insensitive search')
    .option('-l, --limit <number>', 'Maximum number of results', '50')
    .action(searchCommand);

  // Make search the default command
  program
    .argument('[query]', 'Search query (default command)')
    .action((query) => {
      if (query) {
        return searchCommand(query, program.opts());
      }
      program.help();
    });

  try {
    await program.parseAsync(argv);
    return 0;
  } catch (error) {
    console.error(formatError(error));
    return getExitCode(error);
  }
}

// Entry point when run directly
if (import.meta.main) {
  const exitCode = await main(process.argv);
  process.exit(exitCode);
}
```

**Key Features**:
- Global options available to all commands
- Default command (search) for convenience
- Centralized error handling
- Proper exit codes
- Version management

### 2. Search Command (`commands/search.ts`)

The primary command that orchestrates the search workflow.

```typescript
import { parseDoxygenXML } from '../../parser';
import { SearchEngineFactory } from '../../search';
import { FormatterFactory } from '../../search/formatters';
import { loadConfig, mergeConfig } from '../config';
import { validateSearchQuery, validatePath } from '../validators';
import { showSpinner, formatResults } from '../output';

export async function searchCommand(
  query: string,
  options: SearchCommandOptions
): Promise<void> {
  // 1. Load and merge configuration
  const config = await loadConfig(options.config);
  const finalConfig = mergeConfig(config, options);
  
  // 2. Validate inputs
  validateSearchQuery(query);
  validatePath(finalConfig.path);
  
  // 3. Parse Doxygen XML
  const spinner = showSpinner('Parsing Doxygen XML...');
  let parsed;
  try {
    parsed = await parseDoxygenXML(finalConfig.path, {
      useCache: true,
      entityTypes: finalConfig.type === 'all' ? undefined : [finalConfig.type]
    });
    spinner.succeed(`Found ${parsed.entities.length} entities`);
  } catch (error) {
    spinner.fail('Failed to parse Doxygen XML');
    throw new ParseError(`Cannot parse XML: ${error.message}`);
  }
  
  // 4. Execute search
  const searchEngine = SearchEngineFactory.create('simple');
  searchEngine.configure({
    caseSensitive: !finalConfig.caseInsensitive,
    exactMatch: finalConfig.exact,
    maxResults: parseInt(finalConfig.limit)
  });
  
  const results = searchEngine.search({
    term: query,
    entityTypes: finalConfig.type === 'all' ? undefined : [finalConfig.type]
  });
  
  // 5. Format and display results
  const formatter = FormatterFactory.create(finalConfig.format);
  const output = formatter.format(results);
  
  console.log(output);
  
  // 6. Show summary
  if (finalConfig.verbose) {
    console.log(`\nSearch completed in ${elapsed}ms`);
    console.log(`Total entities scanned: ${parsed.entities.length}`);
    console.log(`Results returned: ${results.length}`);
  }
  
  // Exit with appropriate code
  if (results.length === 0) {
    console.log('\nNo results found. Try:');
    console.log('  - Checking your spelling');
    console.log('  - Using a broader search term');
    console.log('  - Removing the --exact flag');
  }
}

interface SearchCommandOptions {
  path?: string;
  format?: string;
  verbose?: boolean;
  config?: string;
  type?: string;
  exact?: boolean;
  caseInsensitive?: boolean;
  limit?: string;
}
```

**Flow**:
1. Load configuration (file + CLI args)
2. Validate all inputs
3. Parse Doxygen XML with spinner feedback
4. Execute search with configured engine
5. Format results
6. Display output with helpful messages

### 3. Configuration Management (`config.ts`)

Handles loading, merging, and validating configuration from multiple sources.

```typescript
import { existsSync } from 'fs';
import { readFile } from 'fs/promises';
import { homedir } from 'os';
import { join } from 'path';

export interface DoxygenSearchConfig {
  // Paths
  doxygenPath: string;
  
  // Search options
  caseSensitive: boolean;
  exactMatch: boolean;
  defaultLimit: number;
  
  // Output options
  outputFormat: 'text' | 'json' | 'table' | 'detailed';
  colorEnabled: boolean;
  verbose: boolean;
  
  // Performance
  cacheEnabled: boolean;
  parallelParsing: boolean;
  
  // Future: AI settings
  aiEnabled?: boolean;
  aiProvider?: string;
  aiModel?: string;
}

const DEFAULT_CONFIG: DoxygenSearchConfig = {
  doxygenPath: './xml',
  caseSensitive: false,
  exactMatch: false,
  defaultLimit: 50,
  outputFormat: 'text',
  colorEnabled: true,
  verbose: false,
  cacheEnabled: true,
  parallelParsing: false
};

/**
 * Load configuration from file
 * Searches in order:
 * 1. Path specified by --config flag
 * 2. .doxysearch.json in current directory
 * 3. .doxysearch.json in home directory
 */
export async function loadConfig(configPath?: string): Promise<Partial<DoxygenSearchConfig>> {
  const searchPaths = configPath 
    ? [configPath]
    : [
        '.doxysearch.json',
        join(process.cwd(), '.doxysearch.json'),
        join(homedir(), '.doxysearch.json')
      ];
  
  for (const path of searchPaths) {
    if (existsSync(path)) {
      try {
        const content = await readFile(path, 'utf-8');
        const config = JSON.parse(content);
        return validateConfig(config);
      } catch (error) {
        throw new ConfigError(`Invalid config file at ${path}: ${error.message}`);
      }
    }
  }
  
  return {};
}

/**
 * Merge configurations with precedence:
 * CLI args > Config file > Defaults
 */
export function mergeConfig(
  fileConfig: Partial<DoxygenSearchConfig>,
  cliOptions: Record<string, any>
): DoxygenSearchConfig {
  return {
    ...DEFAULT_CONFIG,
    ...fileConfig,
    // CLI options override everything
    ...(cliOptions.path && { doxygenPath: cliOptions.path }),
    ...(cliOptions.format && { outputFormat: cliOptions.format }),
    ...(cliOptions.verbose !== undefined && { verbose: cliOptions.verbose }),
    ...(cliOptions.color !== undefined && { colorEnabled: cliOptions.color }),
    // Command-specific options
    ...(cliOptions.caseInsensitive !== undefined && { 
      caseSensitive: !cliOptions.caseInsensitive 
    }),
    ...(cliOptions.exact !== undefined && { exactMatch: cliOptions.exact }),
    ...(cliOptions.limit && { defaultLimit: parseInt(cliOptions.limit) })
  };
}

/**
 * Validate configuration object
 */
function validateConfig(config: any): Partial<DoxygenSearchConfig> {
  const errors: string[] = [];
  
  if (config.outputFormat && !['text', 'json', 'table', 'detailed'].includes(config.outputFormat)) {
    errors.push(`Invalid outputFormat: ${config.outputFormat}`);
  }
  
  if (config.defaultLimit && (config.defaultLimit < 1 || config.defaultLimit > 1000)) {
    errors.push(`Invalid defaultLimit: must be between 1 and 1000`);
  }
  
  if (errors.length > 0) {
    throw new ConfigError(`Config validation failed:\n${errors.join('\n')}`);
  }
  
  return config;
}

export class ConfigError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'ConfigError';
  }
}
```

**Configuration File Example** (`.doxysearch.json`):
```json
{
  "doxygenPath": "./build/docs/xml",
  "caseSensitive": false,
  "defaultLimit": 100,
  "outputFormat": "text",
  "colorEnabled": true,
  "cacheEnabled": true,
  "verbose": false
}
```

### 4. Output Utilities (`output.ts`)

Helpers for formatted, colored terminal output.

```typescript
import chalk from 'chalk';
import ora, { Ora } from 'ora';

let colorsEnabled = true;

export function setColorEnabled(enabled: boolean): void {
  colorsEnabled = enabled;
  chalk.level = enabled ? chalk.level : 0;
}

// Color helpers
export const colors = {
  success: (text: string) => colorsEnabled ? chalk.green(text) : text,
  error: (text: string) => colorsEnabled ? chalk.red(text) : text,
  warning: (text: string) => colorsEnabled ? chalk.yellow(text) : text,
  info: (text: string) => colorsEnabled ? chalk.blue(text) : text,
  dim: (text: string) => colorsEnabled ? chalk.dim(text) : text,
  bold: (text: string) => colorsEnabled ? chalk.bold(text) : text,
  highlight: (text: string) => colorsEnabled ? chalk.cyan.bold(text) : text,
};

// Spinner for long operations
export function showSpinner(text: string): Ora {
  return ora({
    text,
    color: 'cyan',
    spinner: 'dots'
  }).start();
}

// Error formatting
export function formatError(error: Error): string {
  if (error instanceof ConfigError) {
    return colors.error('Configuration Error: ') + error.message;
  }
  
  if (error instanceof ParseError) {
    return colors.error('Parse Error: ') + error.message +
           '\n' + colors.dim('Ensure the path points to valid Doxygen XML output');
  }
  
  if (error instanceof ValidationError) {
    return colors.error('Validation Error: ') + error.message;
  }
  
  // Unknown error
  return colors.error('Error: ') + error.message +
         '\n' + colors.dim(error.stack || '');
}

// Success messages
export function formatSuccess(message: string): string {
  return colors.success('✓ ') + message;
}

export function formatWarning(message: string): string {
  return colors.warning('⚠ ') + message;
}

export function formatInfo(message: string): string {
  return colors.info('ℹ ') + message;
}

// Box for important messages
export function formatBox(title: string, content: string[]): string {
  const maxLength = Math.max(title.length, ...content.map(l => l.length));
  const width = maxLength + 4;
  
  const top = '┌' + '─'.repeat(width - 2) + '┐';
  const titleLine = '│ ' + colors.bold(title) + ' '.repeat(width - title.length - 3) + '│';
  const separator = '├' + '─'.repeat(width - 2) + '┤';
  const contentLines = content.map(line => 
    '│ ' + line + ' '.repeat(width - line.length - 3) + '│'
  );
  const bottom = '└' + '─'.repeat(width - 2) + '┘';
  
  return [top, titleLine, separator, ...contentLines, bottom].join('\n');
}
```

### 5. Input Validators (`validators.ts`)

Validate user inputs before processing.

```typescript
import { existsSync, statSync } from 'fs';
import { resolve } from 'path';

export class ValidationError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'ValidationError';
  }
}

/**
 * Validate search query
 */
export function validateSearchQuery(query: string): void {
  if (!query || query.trim().length === 0) {
    throw new ValidationError('Search query cannot be empty');
  }
  
  if (query.length > 200) {
    throw new ValidationError('Search query is too long (max 200 characters)');
  }
  
  // Check for potentially problematic characters
  if (query.includes('\0')) {
    throw new ValidationError('Search query contains invalid characters');
  }
}

/**
 * Validate Doxygen XML path
 */
export function validatePath(path: string): string {
  if (!path) {
    throw new ValidationError('Doxygen XML path is required. Use --path or set in config file');
  }
  
  const resolvedPath = resolve(path);
  
  if (!existsSync(resolvedPath)) {
    throw new ValidationError(
      `Path does not exist: ${resolvedPath}\n` +
      'Ensure you have run Doxygen with XML output enabled'
    );
  }
  
  const stats = statSync(resolvedPath);
  if (!stats.isDirectory()) {
    throw new ValidationError(
      `Path is not a directory: ${resolvedPath}\n` +
      'The path should point to the XML output directory from Doxygen'
    );
  }
  
  // Check for index.xml as a sanity check
  const indexPath = resolve(resolvedPath, 'index.xml');
  if (!existsSync(indexPath)) {
    throw new ValidationError(
      `No index.xml found in ${resolvedPath}\n` +
      'This does not appear to be a valid Doxygen XML output directory'
    );
  }
  
  return resolvedPath;
}

/**
 * Validate entity type
 */
export function validateEntityType(type: string): void {
  const validTypes = ['class', 'struct', 'function', 'method', 'namespace', 'enum', 'all'];
  
  if (!validTypes.includes(type)) {
    throw new ValidationError(
      `Invalid entity type: ${type}\n` +
      `Valid types are: ${validTypes.join(', ')}`
    );
  }
}

/**
 * Validate output format
 */
export function validateOutputFormat(format: string): void {
  const validFormats = ['text', 'json', 'table', 'detailed'];
  
  if (!validFormats.includes(format)) {
    throw new ValidationError(
      `Invalid output format: ${format}\n` +
      `Valid formats are: ${validFormats.join(', ')}`
    );
  }
}

/**
 * Validate numeric option
 */
export function validateNumericOption(
  value: string,
  name: string,
  min: number,
  max: number
): number {
  const num = parseInt(value, 10);
  
  if (isNaN(num)) {
    throw new ValidationError(`${name} must be a number`);
  }
  
  if (num < min || num > max) {
    throw new ValidationError(`${name} must be between ${min} and ${max}`);
  }
  
  return num;
}
```

### 6. Future Commands (`commands/`)

Stubs for future command implementations:

**`commands/list.ts`** (Phase 2):
```typescript
/**
 * List all entities of a given type
 * Usage: doxygen-search list --type class
 */
export async function listCommand(options: ListOptions): Promise<void> {
  // Implementation for Phase 2
}
```

**`commands/info.ts`** (Phase 2):
```typescript
/**
 * Show detailed information about a specific entity
 * Usage: doxygen-search info MyClass
 */
export async function infoCommand(entityName: string, options: InfoOptions): Promise<void> {
  // Implementation for Phase 2
}
```

**`commands/index.ts`**:
```typescript
export { searchCommand } from './search';
// export { listCommand } from './list';     // Phase 2
// export { infoCommand } from './info';     // Phase 2
```

## Error Handling

### Exit Codes
```typescript
export function getExitCode(error: Error): number {
  if (error instanceof ValidationError) return 1;
  if (error instanceof ConfigError) return 1;
  if (error instanceof ParseError) return 2;
  return 3; // Unknown error
}
```

- **0**: Success
- **1**: User error (invalid arguments, bad config)
- **2**: System error (file not found, parse failure)
- **3**: Internal error (unexpected exception)

### Error Messages

All errors should:
- Be clear and actionable
- Suggest solutions where possible
- Include relevant context
- Be formatted consistently

Example:
```
Error: Path does not exist: /path/to/xml
Ensure you have run Doxygen with XML output enabled.
Try: doxygen -g Doxyfile && doxygen Doxyfile
```

## Testing Strategy

### Unit Tests
```typescript
describe('CLI Configuration', () => {
  test('loads config from file', async () => {
    const config = await loadConfig('./test-config.json');
    expect(config.doxygenPath).toBe('./docs/xml');
  });
  
  test('merges CLI options with config', () => {
    const merged = mergeConfig(
      { doxygenPath: './xml' },
      { path: './override' }
    );
    expect(merged.doxygenPath).toBe('./override');
  });
});

describe('Input Validation', () => {
  test('rejects empty search query', () => {
    expect(() => validateSearchQuery('')).toThrow(ValidationError);
  });
  
  test('validates Doxygen path exists', () => {
    expect(() => validatePath('/nonexistent')).toThrow(ValidationError);
  });
});
```

### Integration Tests
```typescript
describe('Search Command', () => {
  test('executes full search workflow', async () => {
    const result = await searchCommand('MyClass', {
      path: './test-fixtures/xml',
      format: 'json'
    });
    
    expect(result).toContain('MyClass');
  });
});
```

## Dependencies

### Required
- `commander` (^11.0.0) - CLI framework
- `chalk` (^5.0.0) - Terminal colors
- `ora` (^7.0.0) - Spinners

### Optional
- `cli-table3` (^0.6.0) - Table formatting (Phase 2)
- `prompts` (^2.4.0) - Interactive prompts (Phase 3)

## Performance Considerations

1. **Lazy Loading**: Don't parse XML until needed
2. **Progress Feedback**: Show spinners for operations >500ms
3. **Streaming Output**: For large result sets, stream to stdout
4. **Signal Handling**: Handle Ctrl+C gracefully

```typescript
// Handle interruption
process.on('SIGINT', () => {
  console.log('\n' + colors.warning('Search interrupted'));
  process.exit(130);
});
```

## Design Principles

1. **Unix Philosophy**: Do one thing well, compose with other tools
2. **Predictable Behavior**: Consistent arg patterns, exit codes
3. **Helpful Errors**: Guide users to solutions
4. **No Surprises**: Defaults work for common cases
5. **Scriptable**: JSON output, proper exit codes, stderr for errors

## Future Enhancements

- Interactive mode with prompts
- Shell completions (bash, zsh, fish)
- Progress bars for large operations
- Config validation command
- Debug mode with verbose logging
- Watch mode (re-run on file changes)