import chalk from 'chalk';
import { Highlight } from '../types';

/**
 * Highlight matched text in a string
 */
export function highlightText(
  text: string,
  highlights: Highlight[],
  color: (s: string) => string = chalk.cyan.bold
): string {
  if (!highlights || highlights.length === 0) {
    return text;
  }
  
  // Sort highlights by start position
  const sorted = [...highlights].sort((a, b) => a.start - b.start);
  
  let result = '';
  let lastEnd = 0;
  
  for (const highlight of sorted) {
    // Add text before highlight
    result += text.substring(lastEnd, highlight.start);
    
    // Add highlighted text
    const highlightedText = text.substring(
      highlight.start,
      highlight.start + highlight.length
    );
    result += color(highlightedText);
    
    lastEnd = highlight.start + highlight.length;
  }
  
  // Add remaining text
  result += text.substring(lastEnd);
  
  return result;
}

/**
 * Create a context snippet with highlighted matches
 */
export function createSnippet(
  text: string,
  highlight: Highlight,
  contextLength: number = 40
): string {
  const start = Math.max(0, highlight.start - contextLength);
  const end = Math.min(
    text.length,
    highlight.start + highlight.length + contextLength
  );
  
  let snippet = text.substring(start, end);
  
  if (start > 0) snippet = '...' + snippet;
  if (end < text.length) snippet = snippet + '...';
  
  return snippet;
}