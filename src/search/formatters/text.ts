import { ResultFormatter } from './base';
import { SearchResult, SearchStats } from '../types';
import chalk from 'chalk';

export class TextFormatter implements ResultFormatter {
  format(results: SearchResult[], stats?: SearchStats): string {
    if (results.length === 0) {
      return chalk.dim('No results found.');
    }
    
    const lines: string[] = [];
    
    // Header
    lines.push(chalk.bold(`Found ${results.length} result(s):\n`));
    
    // Format each result
    results.forEach((result, index) => {
      lines.push(this.formatResult(result, index + 1));
      lines.push(''); // Blank line between results
    });
    
    // Stats if provided
    if (stats) {
      lines.push(chalk.dim(this.formatStats(stats)));
    }
    
    return lines.join('\n');
  }
  
  private formatResult(result: SearchResult, index: number): string {
    const entity = result.entity;
    const lines: string[] = [];
    
    // 1. Result number and name
    const nameColor = this.getKindColor(entity.kind);
    lines.push(
      `${chalk.cyan(`${index}.`)} ${nameColor(entity.name)} ` +
      chalk.dim(`(${entity.kind})`)
    );
    
    // 2. Location
    lines.push(
      `   ${chalk.dim('File:')} ${entity.file}:${chalk.yellow(entity.line)}`
    );
    
    // 3. Relevance score
    const scorePercent = Math.round(result.relevance * 100);
    lines.push(
      `   ${chalk.dim('Match:')} ${this.formatScore(scorePercent)}`
    );
    
    // 4. Brief description if available
    if (entity.briefDescription) {
      const desc = this.truncate(entity.briefDescription, 80);
      lines.push(`   ${chalk.dim(desc)}`);
    }
    
    // 5. Matched fields
    if (result.matchedFields.length > 0) {
      const fields = result.matchedFields
        .map(f => f.fieldName)
        .join(', ');
      lines.push(`   ${chalk.dim('Matched in:')} ${fields}`);
    }
    
    return lines.join('\n');
  }
  
  private getKindColor(kind: string): (s: string) => string {
    switch (kind) {
      case 'class':
      case 'struct':
        return chalk.green;
      case 'function':
      case 'method':
        return chalk.blue;
      case 'namespace':
        return chalk.magenta;
      default:
        return chalk.white;
    }
  }
  
  private formatScore(percent: number): string {
    if (percent >= 90) return chalk.green(`${percent}%`);
    if (percent >= 70) return chalk.yellow(`${percent}%`);
    return chalk.red(`${percent}%`);
  }
  
  private formatStats(stats: SearchStats): string {
    return [
      `Searched ${stats.totalEntities} entities`,
      `in ${stats.searchTimeMs.toFixed(2)}ms`,
      stats.cacheHit ? '(cached)' : ''
    ].filter(Boolean).join(' ');
  }
  
  private truncate(text: string, maxLength: number): string {
    if (text.length <= maxLength) return text;
    return text.substring(0, maxLength - 3) + '...';
  }
}