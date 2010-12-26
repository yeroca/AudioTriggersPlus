/*
 * parse_xml.c
 *
 *  Created on: Dec 25, 2010
 *      Author: corey
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <libxml/parser.h>
#include <libxml/xmlschemas.h>

#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>


/* This code is from http://wiki.njh.eu/XML-Schema_validation_with_libxml2 */
static int is_valid(const xmlDocPtr doc, const char *schema_filename)
{
	xmlDocPtr schema_doc = xmlReadFile(schema_filename, NULL,
			XML_PARSE_NONET);
	if (schema_doc == NULL) {
		/* the schema cannot be loaded or is not well-formed */
		return -1;
	}
	xmlSchemaParserCtxtPtr parser_ctxt = xmlSchemaNewDocParserCtxt(
			schema_doc);
	if (parser_ctxt == NULL) {
		/* unable to create a parser context for the schema */
		xmlFreeDoc(schema_doc);
		return -2;
	}
	xmlSchemaPtr schema = xmlSchemaParse(parser_ctxt);
	if (schema == NULL) {
		/* the schema itself is not valid */
		xmlSchemaFreeParserCtxt(parser_ctxt);
		xmlFreeDoc(schema_doc);
		return -3;
	}
	xmlSchemaValidCtxtPtr valid_ctxt = xmlSchemaNewValidCtxt(schema);
	if (valid_ctxt == NULL) {
		/* unable to create a validation context for the schema */
		xmlSchemaFree(schema);
		xmlSchemaFreeParserCtxt(parser_ctxt);
		xmlFreeDoc(schema_doc);
		return -4;
	}
	int is_valid = (xmlSchemaValidateDoc(valid_ctxt, doc) == 0);
	xmlSchemaFreeValidCtxt(valid_ctxt);
	xmlSchemaFree(schema);
	xmlSchemaFreeParserCtxt(parser_ctxt);
	xmlFreeDoc(schema_doc);
	/* force the return value to be non-negative on success */
	return is_valid ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc != 3)
		return (1);

	/*
	 * this initialize the library and check potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used.
	 */
	LIBXML_TEST_VERSION

	xmlDocPtr doc = xmlReadFile(argv[1], NULL, XML_PARSE_NONET);
	if (doc == NULL) {
		/* the doc cannot be loaded or is not well-formed */
		fprintf(stderr, "Unable to load doc file\n");
		return -1;
	}

	printf("is valid = %d\n", is_valid(doc, argv[2]));

	/*
	 * Cleanup function for the XML library.
	 */
	xmlCleanupParser();
	/*
	 * this is to debug memory for regression tests
	 */
	xmlMemoryDump();
	return (0);
}
