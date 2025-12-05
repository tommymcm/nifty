import { SearchResult } from '../types';

/**
 * Interface for result formatters
 */
export interface ResultFormatter {
  format(results: SearchResult[], stats?: SearchStats): string;
}

/**
 * Factory for creating formatters
 */
export class FormatterFactory {
  static create(format: 'text' | 'json' | 'table' | 'detailed'): ResultFormatter {
    switch (format) {
      case 'text':
        return new TextFormatter();
      case 'json':
        return new JSONFormatter();
      case 'table':
        return new TableFormatter();
      case 'detailed':
        return new DetailedFormatter();
      default:
        throw new Error(`Unknown format: ${format}`);
    }
  }
}