#include "yaml/yaml.h"
#include "util/yaml.h"
#include "util/linked_list.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/ptree.h"
#include "util/unordered_vector.h"
#include <assert.h>

#ifdef UTIL_PLATFORM_MACOSX
#   include "util/platform/osx/fmemopen.h"
#endif

static struct list_t g_open_docs;

static char
yaml_load_into_ptree(struct ptree_t* tree,
    				 struct ptree_t* root_node,
    				 yaml_parser_t* parser,
    				 char is_sequence);

static char*
yaml_dup_node_value_func(char* value);

static void
yaml_free_node_value_func(char* value);

static void
yaml_init_node(struct ptree_t* node);

/* ------------------------------------------------------------------------- */
void
yaml_init(void)
{
    list_init(&g_open_docs);
}

/* ------------------------------------------------------------------------- */
void
yaml_deinit(void)
{
    list_clear(&g_open_docs);
}

/* ------------------------------------------------------------------------- */
struct ptree_t*
yaml_create(void)
{
    return ptree_create(NULL);
}

/* ------------------------------------------------------------------------- */
struct ptree_t*
yaml_load(const char* filename)
{
    FILE* fp;
    struct ptree_t* doc;

    assert(filename);

    /* try to open the file */
    fp = fopen(filename, "rb");
    if(!fp)
    {
    	fprintf(stderr, "Failed to open file \"%s\"\n", filename);
    	return NULL;
    }

    doc = yaml_load_from_stream(fp);
    fclose(fp);

    return doc;
}

/* ------------------------------------------------------------------------- */
#if defined(UTIL_PLATFORM_LINUX) || defined(LIGHTSHIP_UTIL_PLATFORM_MACOSX)
struct ptree_t*
yaml_load_from_memory(const char* buffer)
{
    FILE* stream;
    struct ptree_t* doc;

    assert(buffer);

    stream = fmemopen((char*)buffer, strlen(buffer), "rb");
    if(!stream)
    {
    	fprintf(stderr, "Failed to open buffer as stream");
    	return NULL;
    }

    doc = yaml_load_from_stream(stream);
    fclose(stream);

    return doc;
}
#endif

/* ------------------------------------------------------------------------- */
struct ptree_t*
yaml_load_from_stream(FILE* stream)
{
    yaml_parser_t parser;
    struct ptree_t* doc = NULL;

    assert(stream);

    /* init parser */
    if(!yaml_parser_initialize(&parser))
    	return NULL;
    yaml_parser_set_input_file(&parser, stream);

    for(;;)
    {
    	/* parse file and load into tree */
    	if(!(doc = ptree_create(NULL)))
    		break;
    	yaml_init_node(doc);
    	if(!yaml_load_into_ptree(doc, doc, &parser, 0))
    	{
    		fprintf(stderr, "Syntax error: Failed to parse YAML.\n");
    		break;
    	}

    	/* add to open documents */
    	if(!list_push(&g_open_docs, doc))
    		break;

    	yaml_parser_delete(&parser);

    	return doc;
    }

    /* clean up */
    yaml_parser_delete(&parser);
    if(doc)
    	ptree_destroy(doc);

    return NULL;
}

/* ------------------------------------------------------------------------- */
void
yaml_destroy(struct ptree_t* doc)
{
    assert(doc);
    ptree_destroy(doc);
    list_erase_element(&g_open_docs, doc);
}

/* ------------------------------------------------------------------------- */
const char*
yaml_get_value(const struct ptree_t* doc, const char* key)
{
    struct ptree_t* node;

    assert(doc);
    assert(key);

    if(!(node = yaml_get_node(doc, key)))
    	return NULL;

    /* return the last item in the vector */
    return (const char*)node->value;
}

/* ------------------------------------------------------------------------- */
struct ptree_t*
yaml_get_node(const struct ptree_t* node, const char* key)
{
    assert(node);
    assert(key);

    return ptree_get_node(node, key);
}

/* ------------------------------------------------------------------------- */
struct ptree_t*
yaml_set_value(struct ptree_t* doc, const char* key, const char* value)
{
    struct ptree_t* node;
    char* value_cpy = NULL;

    assert(doc);
    assert(key);

    if(value)
    	if(!(value_cpy = malloc_string(value)))
    		return NULL;

    if(!(node = ptree_set(doc, key, value_cpy)))
    {
    	if(value_cpy)
    		free_string(value_cpy);
    	return NULL;
    }

    yaml_init_node(node);

    return node;
}

/* ------------------------------------------------------------------------- */
static void
yaml_init_node(struct ptree_t* node)
{
    /* init duplication and free functions */
    ptree_set_dup_func(node, (ptree_dup_func)yaml_dup_node_value_func);
    ptree_set_free_func(node, (ptree_free_func)yaml_free_node_value_func);
}

/* ------------------------------------------------------------------------- */
/*
 * Takes the value of a yaml node (node->value) and duplicates it.
 */
static char*
yaml_dup_node_value_func(char* value)
{
    return malloc_string(value);
}

/* ------------------------------------------------------------------------- */
/*
 * Takes the value of a yaml node (node->value) and frees it.
 */
static void
yaml_free_node_value_func(char* value)
{
    free_string(value);
}

/* ------------------------------------------------------------------------- */
char
yaml_remove_value(struct ptree_t* doc, const char* key)
{
    return ptree_remove(doc, key);
}

/* ------------------------------------------------------------------------- */
char
yaml_string_to_bool(const char* str)
{
    const char* p_source;
    const char* p_comp;
    static const char* comp = "true";
    for(p_source = str, p_comp = comp; *p_source && *p_comp; ++p_source, ++p_comp)
    {
    	/* case insensitive check */
    	if(*p_source != *p_comp && (*p_source | 0x60) != *p_comp)
    		return 0;
    }

    return 1;
}

/* ------------------------------------------------------------------------- */
static char
yaml_load_into_ptree(struct ptree_t* tree,
    				 struct ptree_t* root_node,
    				 yaml_parser_t* parser,
    				 char is_sequence)
{
    yaml_event_t event;
    char* key;
    char finished = 0;
    char sequence_index = 0; /* this is used to generate keys for when lists/
    						  * sequences are read, as the ptree requires each
    						  * node to have a key. */
    const char FINISH_ERROR = 1;
    const char FINISH_SUCCESS = 2;

    if(!yaml_parser_parse(parser, &event))
    {
    	fprintf(stderr, "[yaml] Parser error: %d\n", parser->error);
    	return 0;
    }

    key = NULL;
    while(event.type != YAML_STREAM_END_EVENT)
    {
    	char result;

    	switch(event.type)
    	{
    		case YAML_NO_EVENT:
    			fprintf(stderr, "[yaml] Syntax error in yaml document\n");
    			finished = FINISH_ERROR;
    			break;

    		/* stream/document start/end */
    		case YAML_STREAM_START_EVENT:
    		case YAML_STREAM_END_EVENT:
    		case YAML_DOCUMENT_START_EVENT:
    		case YAML_DOCUMENT_END_EVENT:
    			break;

    		/* begin of sequence (yaml list) */
    		case YAML_SEQUENCE_START_EVENT:
    			if(key)
    			{
    				/* create child and recurse, setting is_sequence to 1 so
    				 * the parser knows to generate sequence keys */
    				struct ptree_t* child;
    				if(!(child = yaml_set_value(tree, key, NULL)) ||
    				   !(result = yaml_load_into_ptree(child, root_node, parser, 1)))
    				{
    					finished = FINISH_ERROR;
    				}

    				free_string(key);
    				key = NULL;
    			}
    			else
    			{
    				fprintf(stderr, "[yaml] Received sequence start without a key\n");
    				finished = FINISH_ERROR;
    				break;
    			}

    		case YAML_MAPPING_START_EVENT:

    			/*
    			 * If this is a sequence, create an index key.
    			 */
    			if(is_sequence && !key)
    			{
    				if(!(key = malloc_string("255"))) /* sequence_index is a char */
    				{
    					finished = FINISH_ERROR;
    					break;
    				}
    				sprintf(key, "%d", sequence_index);
    				++sequence_index;
    			}

    			/*
    			 * If this is not a sequence, then only recurse if a key
    			 * exists.
    			 */
    			if(key)
    			{
    				struct ptree_t* child;
    				if(!(child = yaml_set_value(tree, key, NULL)) ||
    				   !(result = yaml_load_into_ptree(child, root_node, parser, 0)))
    				{
    					finished = FINISH_ERROR;
    				}

    				free_string(key);
    				key = NULL;
    			}
    			break;

    		/* end of sequence or mapping */
    		case YAML_SEQUENCE_END_EVENT:
    		case YAML_MAPPING_END_EVENT:
    			finished = FINISH_SUCCESS;
    			break;

    		/*
    		 * Aliases - Find the anchor name in the root node, and recursively
    		 * copy said node into the current tree.
    		 */
    		case YAML_ALIAS_EVENT:

    			/*
    			 * If this is a sequence, create an index key.
    			 */
    			if(is_sequence && !key)
    			{
    				if(!(key = malloc_string("255"))) /* sequence_index is a char */
    				{
    					finished = FINISH_ERROR;
    					break;
    				}
    				sprintf(key, "%d", sequence_index);
    				++sequence_index;
    			}

    			if(key)
    			{
    				const struct ptree_t* source = yaml_get_node(root_node, (char*)event.data.alias.anchor);
    				if(source)
    				{
    					struct ptree_t* child;
    					if(!(child = yaml_set_value(tree, key, NULL)) ||
    					   !ptree_duplicate_children_into_existing_node(child, source))
    					{
    						fprintf(stderr, "[yaml] Failed to duplicate tree (anchor copy failed)\n");
    						fprintf(stderr, "[yaml] Make sure you're not using the same key more than once\n");
    						finished = FINISH_ERROR;
    					}
    				}
    				else
    				{
    					fprintf(stderr, "[yaml] Failed to copy ptree \"%s\" (yaml anchor failed)\n", event.data.alias.anchor);
    					fprintf(stderr, "[yaml] Possible solution: References need to be defined before they are used.\n");
    					finished = FINISH_ERROR;
    				}
    				free_string(key);
    				key = NULL;
    			}
    			break;

    		/* scalar */
    		case YAML_SCALAR_EVENT:

    			/*
    			 * If the scalar doesn't belong to a sequence, simply toggle
    			 * back and forth between key and value, creating a new node
    			 * each time a value is received.
    			 * If the scalar does belong to a sequence, use the current
    			 * sequence index as the key instead.
    			 */
    			if(is_sequence)
    			{
    				char index[sizeof(int)*8+1];
    				if(key)
    				{
    					fprintf(stderr, "[yaml] Received a key during a sequence\n");
    					finished = FINISH_ERROR;
    					break;
    				}
    				sprintf(index, "%d", sequence_index);
    				yaml_set_value(tree, index, (char*)event.data.scalar.value);
    				++sequence_index;
    			}
    			else /* scalar doesn't belong to a sequence */
    			{
    				if(key)
    				{
    					yaml_set_value(tree, key, (char*)event.data.scalar.value);
    					free_string(key);
    					key = NULL;
    				}
    				else
    				{
    					if(!(key = malloc_string((char*)event.data.scalar.value)))
    						finished = FINISH_ERROR;
    				}
    			}
    			break;

    		default:
    			fprintf(stderr, "[yaml] Unknown error\n");
    			finished = FINISH_ERROR;
    			break;

    	}

    	if(finished)
    		break;

    	yaml_event_delete(&event);
    	yaml_parser_parse(parser, &event);
    }

    /* clean up */
    yaml_event_delete(&event);
    if(key)
    	free_string(key);

    if(finished == FINISH_ERROR)
    	return 0;

    return 1;
}
