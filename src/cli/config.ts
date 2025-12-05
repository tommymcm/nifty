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