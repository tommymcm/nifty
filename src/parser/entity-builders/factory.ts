import { EntityBuilder } from './base';
import { ClassEntityBuilder } from './class-builder';
import { FunctionEntityBuilder } from './function-builder';
import { NamespaceEntityBuilder } from './namespace-builder';
import { EnumEntityBuilder } from './enum-builder';

export class EntityBuilderFactory {
  /**
   * Get appropriate builder for entity kind
   */
  static getBuilder(kind: string): EntityBuilder {
    switch (kind) {
      case 'class':
      case 'struct':
        return new ClassEntityBuilder();
      
      case 'function':
        return new FunctionEntityBuilder();
      
      case 'namespace':
        return new NamespaceEntityBuilder();
      
      case 'enum':
        return new EnumEntityBuilder();
      
      default:
        throw new Error(`Unsupported entity kind: ${kind}`);
    }
  }
}