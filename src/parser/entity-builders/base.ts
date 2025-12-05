
import { DoxygenEntity } from '../types';

/**
 * Base class for entity builders
 */
export abstract class EntityBuilder<T extends DoxygenEntity = DoxygenEntity> {
  /**
   * Build entity from XML node
   */
  abstract build(xmlNode: any, parent?: DoxygenEntity): T;
  
  /**
   * Extract common fields shared by all entities
   */
  protected buildBase(xmlNode: any): Partial<DoxygenEntity> {
    return {
      id: xmlNode['@_id'] || '',
      name: this.extractName(xmlNode),
      qualifiedName: this.extractQualifiedName(xmlNode),
      file: this.extractFile(xmlNode),
      line: this.extractLine(xmlNode),
      briefDescription: this.extractBriefDescription(xmlNode),
      detailedDescription: this.extractDetailedDescription(xmlNode),
      visibility: this.extractVisibility(xmlNode)
    };
  }
  
  protected extractName(xmlNode: any): string {
    return xmlNode.compoundname || xmlNode.name || '';
  }
  
  protected extractQualifiedName(xmlNode: any): string {
    return xmlNode.qualifiedname || xmlNode.compoundname || xmlNode.name || '';
  }
  
  protected extractFile(xmlNode: any): string {
    const location = xmlNode.location;
    return location?.['@_file'] || '';
  }
  
  protected extractLine(xmlNode: any): number {
    const location = xmlNode.location;
    const line = location?.['@_line'];
    return line ? parseInt(line, 10) : 0;
  }
  
  protected extractBriefDescription(xmlNode: any): string | undefined {
    const brief = xmlNode.briefdescription;
    if (!brief) return undefined;
    
    return this.extractText(brief);
  }
  
  protected extractDetailedDescription(xmlNode: any): string | undefined {
    const detailed = xmlNode.detaileddescription;
    if (!detailed) return undefined;
    
    return this.extractText(detailed);
  }
  
  protected extractVisibility(xmlNode: any): 'public' | 'protected' | 'private' | undefined {
    const prot = xmlNode['@_prot'];
    if (prot === 'public' || prot === 'protected' || prot === 'private') {
      return prot;
    }
    return undefined;
  }
  
  /**
   * Extract text content from XML node
   */
  protected extractText(node: any): string {
    if (typeof node === 'string') return node;
    if (!node) return '';
    
    // Handle para elements
    if (node.para) {
      const paras = Array.isArray(node.para) ? node.para : [node.para];
      return paras
        .map(p => this.extractText(p))
        .join('\n')
        .trim();
    }
    
    // Handle text nodes
    if (node['#text']) {
      return node['#text'];
    }
    
    // Handle mixed content
    if (typeof node === 'object') {
      return Object.values(node)
        .map(v => this.extractText(v))
        .join(' ')
        .trim();
    }
    
    return '';
  }
}