import { EntityBuilder } from './base';
import { FunctionEntity, Parameter, DoxygenEntity } from '../types';
import { extractParameters } from '../extractors';

export class FunctionEntityBuilder extends EntityBuilder<FunctionEntity> {
  build(xmlNode: any, parent?: DoxygenEntity): FunctionEntity {
    const base = this.buildBase(xmlNode);
    
    // Determine if this is a method or standalone function
    const kind = parent?.kind === 'class' || parent?.kind === 'struct'
      ? 'method'
      : 'function';
    
    return {
      ...base,
      kind,
      returnType: this.extractReturnType(xmlNode),
      parameters: extractParameters(xmlNode),
      isConst: xmlNode['@_const'] === 'yes',
      isStatic: xmlNode['@_static'] === 'yes',
      isVirtual: xmlNode['@_virt'] === 'virtual' || xmlNode['@_virt'] === 'pure-virtual',
      isInline: xmlNode['@_inline'] === 'yes',
      isPureVirtual: xmlNode['@_virt'] === 'pure-virtual',
      parentClass: kind === 'method' ? parent?.qualifiedName : undefined,
      namespace: this.extractNamespace(base.qualifiedName),
      exceptions: this.extractExceptions(xmlNode)
    } as FunctionEntity;
  }
  
  private extractReturnType(xmlNode: any): string {
    const type = xmlNode.type;
    if (!type) return 'void';
    
    return this.extractText(type);
  }
  
  private extractNamespace(qualifiedName: string): string | undefined {
    const lastColon = qualifiedName.lastIndexOf('::');
    if (lastColon === -1) return undefined;
    return qualifiedName.substring(0, lastColon);
  }
  
  private extractExceptions(xmlNode: any): string[] | undefined {
    // Doxygen can document exceptions in the description
    // This is a simplified extraction
    const detailed = xmlNode.detaileddescription;
    if (!detailed) return undefined;
    
    // Look for @throws or @exception tags
    // This is a placeholder - actual implementation would parse XML structure
    return undefined;
  }
}