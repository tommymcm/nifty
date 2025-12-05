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