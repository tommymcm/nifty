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