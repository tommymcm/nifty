import { EntityBuilder } from './base';
import { NamespaceEntity, MemberReference } from '../types';
import { extractMembers } from '../extractors/members';

/**
 * Builder for namespace entities
 * Handles parsing of namespace definitions from Doxygen XML
 */
export class NamespaceEntityBuilder extends EntityBuilder<NamespaceEntity> {
  /**
   * Build a NamespaceEntity from XML node
   * @param xmlNode - The parsed XML node containing namespace definition
   * @returns Complete NamespaceEntity object
   */
  build(xmlNode: any): NamespaceEntity {
    const base = this.buildBase(xmlNode);
    
    return {
      ...base,
      kind: 'namespace',
      members: extractMembers(xmlNode),
      parentNamespace: this.extractParentNamespace(base.qualifiedName || base.name)
    } as NamespaceEntity;
  }
  
  /**
   * Extract parent namespace from qualified name
   * e.g., "std::chrono" -> "std"
   *       "my::nested::ns" -> "my::nested"
   *       "global" -> undefined
   */
  private extractParentNamespace(qualifiedName: string): string | undefined {
    const lastColonIndex = qualifiedName.lastIndexOf('::');
    
    // No parent namespace (global namespace)
    if (lastColonIndex === -1) {
      return undefined;
    }
    
    // Extract parent namespace
    return qualifiedName.substring(0, lastColonIndex);
  }
}
