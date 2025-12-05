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