#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* 
 * The usage information of the program
 */
#define USAGE                                                       \
    "usage: graph [EQUATION] [FLAG]\n"                              \
    "flags:\n"                                                      \
    "  -h      - Display this help message and exit\n"              \
    "  -r ROWS - Set the number of rows in the graph\n"             \
    "  -c COLS - Set the number of columns in the graph\n"          \
    "  -o PATH - Set the path of the output file\n"                 \
    "  -f FORE - Set the color of the foreground in the graph\n"    \
    "  -b BACK - Set the color of the background in the graph\n"    \

/*
 * Get the maximum between two values
 */
#define MAX(a, b) ((a > b) ? a : b)

/*
 * Get the maximum between two values
 */
#define MIN(a, b) ((a < b) ? a : b)

/*
 * Assert a condition
 */
#define ASSERT(condition, ...)                  \
    if (!(condition)) {                         \
        fprintf(stderr, "error: ");             \
        fprintf(stderr, __VA_ARGS__);           \
        fprintf(stderr, "\n");                  \
        graph_free(graph);                      \
        exit(1);                                \
    }

/*
 * Grow an array to a particular length
 */
#define GROW_ARRAY(type, array, length, capacity, min)      \
    if (length >= capacity) {                               \
        capacity = (capacity >= min) ? capacity * 2 : min;  \
        array = (array)                                     \
            ? realloc(array, sizeof(type) * capacity)       \
            : malloc(sizeof(type) * capacity);              \
    }

/*
 * A term of algebraic polynomial
 */
typedef struct {
    float scale;
    size_t power;
} Term;

/*
 * An algebraic polynomial which represents a line in 2D space
 */
typedef struct {
    Term *terms;
    size_t count;
    size_t capacity;
} Line;

/*
 * The minimum number of terms in a line
 * Set to a sufficiently high value to avoid repeated realloc() calls,
 * which has a high time complexity.
 */
#define MINIMUM_LINE_CAPACITY 8

/*
 * A graph in 2D space
 */
typedef struct {
    Line *lines;
    size_t count;
    size_t capacity;

    size_t rows;
    size_t cols;
    uint32_t fore;
    uint32_t back;
    char *path;
    bool *grid;
} Graph;

/*
 * The minimum number of lines in a graph
 * Set to a sufficiently high value to avoid repeated realloc() calls,
 * which has a high time complexity.
 */
#define MINIMUM_GRAPH_CAPACITY 128

/*
 * Skip the whitespace in a string
 * @param expr **char The string to skip the whitespace in
 */
void skip_whitespace(char **expr)
{
    while (isspace(**expr)) *expr += 1;
}

/*
 * Parse a scalar from the beginning of a string
 * @param expr **char The string to parse the scalar from
 * @return float The parsed scalar or 1.0 incase of parsing failure
 */
float parse_scale(char **expr)
{
    skip_whitespace(expr);
    return (isdigit(**expr) || **expr == '-') ? strtof(*expr, expr) : 1.0;
}

/*
 * Parse 'x'power from the beginning of a string
 * @param expr **char The string to parse the power from
 * @return size_t The parsed power or 0 incase of parsing failure
 */
size_t parse_power(char **expr)
{
    skip_whitespace(expr);
    if (**expr != 'x') return 0;
    *expr += 1;
    skip_whitespace(expr);
    return (isdigit(**expr)) ? strtod(*expr, expr) : 1;
}

/*
 * Parse a term from a string
 * @param term *Term The term to parse into
 * @param expr **char The string to parse from
 * @return bool If the end of the polynomial has been reached
 */
bool term_parse(Term *term, char **expr)
{
    skip_whitespace(expr);

    switch (**expr) {
    case '-':
        *expr += 1;
        term->scale = -1.0 * parse_scale(expr);
        break;
    case '+':
        *expr += 1;
        term->scale = parse_scale(expr);
        break;
    case '\0':
        return false;
    default:
        term->scale = parse_scale(expr);
        break;
    }

    term->power = parse_power(expr);
    return true;
}

/*
 * Solve a term for a value of 'x'
 * @param term Term The term to solve
 * @param x float The value of 'x' to solve for
 * @return float The solved value
 */
float term_solve(Term term, float x)
{
    return term.scale * powf(x, term.power);
}

/*
 * Append a term to a line
 * @param line *Line The line to append to term to
 * @param term Term The term to append to the line
 */
void line_append(Line *line, Term term)
{
    GROW_ARRAY(Term,
               line->terms,
               line->count,
               line->capacity,
               MINIMUM_LINE_CAPACITY);

    line->terms[line->count++] = term;
}

/*
 * Deallocate all memory in a line
 * @param line *Line The line to free
 */
void line_free(Line *line)
{
    if (line->terms) free(line->terms);
    line->count = line->capacity = 0;
}

/*
 * Parse a string into a line
 * @param line *Line The line to parse the string into
 * @param expr **char The string to parse
 */
void line_parse(Line *line, char **expr)
{
    Term term = {0};
    while (term_parse(&term, expr)) line_append(line, term);
}

/*
 * Solve a line for a value of 'x'
 * @param line *Line The line to solve
 * @param x float The value to solve for
 * @return float The solved result
 */
float line_solve(Line line, float x)
{
    float result = 0.0;

    for (size_t i = 0; i < line.count; ++i)
        result += term_solve(line.terms[i], x);

    return result;
}

/*
 * Append a line to a graph
 * @param graph *Graph The graph to append to line to
 * @param line Line The line to append to the graph
 */
void graph_append(Graph *graph, Line line)
{
    GROW_ARRAY(Line,
               graph->lines,
               graph->count,
               graph->capacity,
               MINIMUM_GRAPH_CAPACITY);

    graph->lines[graph->count++] = line;
}

/*
 * Deallocate all memory in a graph
 * @param graph *Graph The graph to free
 */
void graph_free(Graph *graph)
{
    if (graph->lines) {
        for (size_t i = 0; i < graph->count; ++i)
            line_free(&graph->lines[i]);
        free(graph->lines);
    }

    if (graph->grid) free(graph->grid);
}

/*
 * Parse the command line arguments into a graph
 * @param graph *Graph The graph to parse the arguments into
 * @param argc size_t The number of command line arguments
 * @param argv **char The command line arguments
 */
void graph_args(Graph *graph, size_t argc, char **argv)
{
    graph->rows = 100;
    graph->cols = 100;
    graph->fore = 0x93E0E3;
    graph->back = 0x3F3F3F;
    graph->path = "output.ppm";

    for (size_t i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-r") == 0) {
            ASSERT(argc > ++i, "ROWS not specified [-r]\n" USAGE);
            graph->rows = atoi(argv[i]);
            ASSERT(graph->rows != 0, "invalid number of rows: '%s'", argv[i]);
        } else if (strcmp(argv[i], "-c") == 0) {
            ASSERT(argc > ++i, "COLS not specified [-c]\n" USAGE);
            graph->cols = atoi(argv[i]);
            ASSERT(graph->cols != 0, "invalid number of columns: '%s'", argv[i]);
        } else if (strcmp(argv[i], "-f") == 0) {
            ASSERT(argc > ++i, "FORE not specified [-f]\n" USAGE);

            int parsed = sscanf((*argv[i] == '#') ? argv[i] + 1 : argv[i],
                                "%x", &graph->back);

            ASSERT(parsed == 1, "invalid foreground color: '%s'", argv[i]);
        } else if (strcmp(argv[i], "-b") == 0) {
            ASSERT(argc > ++i, "BACK not specified [-b]\n" USAGE);

            int parsed = sscanf((*argv[i] == '#') ? argv[i] + 1 : argv[i],
                                "%x", &graph->back);

            ASSERT(parsed == 1, "invalid background color: '%s'", argv[i]);
        } else if (strcmp(argv[i], "-o") == 0) {
            ASSERT(argc > ++i, "PATH not specified [-o]\n" USAGE);
            graph->path = argv[i];
        } else if (strcmp(argv[i], "-h") == 0) {
            fprintf(stdout, USAGE);
            exit(0);
        } else {
            ASSERT(*argv[i] != '-', "invalid flag '%s'", argv[i]);

            Line line = {0};
            line_parse(&line, &argv[i]);
            graph_append(graph, line);
        }
    }

    graph->grid = calloc(graph->rows * graph->cols, sizeof(bool));
}

/*
 * Render the grid of a graph
 * @param graph *Graph The graph to render
 */
void graph_draw(Graph *graph)
{
    float dx = graph->cols / 2.0;
    float dy = graph->rows / 2.0;

    for (size_t i = 0; i < graph->count; ++i) {
        bool drawn = false;
        size_t ly = 0;

        for (size_t x = 0; x < graph->cols; ++x) {
            size_t y = dy - line_solve(graph->lines[i], x - dx);

            if (y < graph->rows) {
                if (drawn) {
                    for (size_t ay = MIN(y, ly); ay < MAX(y, ly); ++ay) {
                        if (ay < graph->rows) {
                            graph->grid[ay * graph->cols + x] = true;
                        }
                    }
                } else {
                    graph->grid[y * graph->cols + x] = true;
                    drawn = true;
                }

                ly = y;
            }
        }
    }
}

/*
 * Save the graph to a PPM image
 * @param graph *Graph The graph to draw
 */
void graph_save(Graph *graph)
{
    FILE *file = fopen(graph->path, "w");
    ASSERT(file, "could not write '%s'", graph->path);

    fprintf(file, "P3 %zu %zu 255\n", graph->cols, graph->rows);
    for (size_t y = 0; y < graph->rows; ++y) {
        for (size_t x = 0; x < graph->cols; ++x) {
            uint32_t face = (graph->grid[y * graph->cols + x])
                ? graph->fore
                : graph->back;

            fprintf(file, "%u %u %u ",
                    (face >> 8 * 2) & 0xff,
                    (face >> 8 * 1) & 0xff,
                    (face >> 8 * 0) & 0xff);
        }
        fprintf(file, "\n");
    }

    fclose(file);
}

int main(int argc, char **argv)
{
    Graph graph = {0};
    graph_args(&graph, argc, argv);
    graph_draw(&graph);
    graph_save(&graph);
    graph_free(&graph);
    return 0;
}
