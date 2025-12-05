import { EntityBuilder } from './base';
import { EnumEntity, EnumValue } from '../types';

/**
 * Builder for enum entities
 * Handles parsing of enum and enum class definitions from Doxygen XML
 */
export class EnumEntityBuilder extends EntityBuilder<EnumEntity> {
  /**
   * Build an EnumEntity from XML node
   * @param xmlNode - The parsed XML node containing enum definition
   * @returns Complete EnumEntity object
   */
  build(xmlNode: any): EnumEntity {
    const base = this.buildBase(xmlNode);
    
    return {
      ...base,
      kind: 'enum',
      values: this.extractEnumValues(xmlNode),
      isStrong: this.checkIsStrongEnum(xmlNode),
      underlyingType: this.extractUnderlyingType(xmlNode)
    } as EnumEntity;
  }
  
  /**
   * Extract enum values (enumerators) from XML node
   * @returns Array of enum values with names, explicit values, and descriptions
   */
  private extractEnumValues(xmlNode: any): EnumValue[] {
    // Enum values are stored in enumvalue elements
    const enumValues = xmlNode.enumvalue;
    
    if (!enumValues) {
      return [];
    }
    
    // Handle both single and multiple enum values
    const valueList = Array.isArray(enumValues) ? enumValues : [enumValues];
    
    return valueList
      .filter(Boolean)
      .map(enumVal => this.buildEnumValue(enumVal));
  }
  
  /**
   * Build a single enum value object
   */
  private buildEnumValue(enumValNode: any): EnumValue {
    return {
      name: enumValNode.name || '',
      value: this.extractExplicitValue(enumValNode),
      description: this.extractBriefDescription(enumValNode)
    };
  }
  
  /**
   * Extract explicit value assigned to enum value
   * e.g., RED = 0xFF0000
   * @returns The explicit value as a string, or undefined if not specified
   */
  private extractExplicitValue(enumValNode: any): string | undefined {
    const initializer = enumValNode.initializer;
    
    if (!initializer) {
      return undefined;
    }
    
    // The initializer typically contains "= value"
    // Extract just the value part
    let value = this.extractText(initializer);
    
    // Remove leading "=" and whitespace
    value = value.replace(/^\s*=\s*/, '').trim();
    
    return value || undefined;
  }
  
  /**
   * Check if this is a strongly-typed enum (enum class in C++)
   * @returns true if this is an enum class, false for regular enum
   */
  private checkIsStrongEnum(xmlNode: any): boolean {
    // Doxygen marks enum classes with the "strong" attribute
    const strong = xmlNode['@_strong'];
    return strong === 'yes';
  }
  
  /**
   * Extract underlying type for the enum
   * e.g., enum class Color : uint8_t
   * @returns The underlying type as a string, or undefined if not specified
   */
  private extractUnderlyingType(xmlNode: any): string | undefined {
    const type = xmlNode.type;
    
    if (!type) {
      return undefined;
    }
    
    const typeStr = this.extractText(type);
    
    // Empty type means default underlying type (typically int)
    if (!typeStr || typeStr.trim() === '') {
      return undefined;
    }
    
    return typeStr.trim();
  }
}