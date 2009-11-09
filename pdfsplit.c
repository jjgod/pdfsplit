// pdfsplit.c: split PDF into pages with Core Graphics framework
// A direct translation of http://www.cs.cmu.edu/~benhdj/Mac/splitPDF.py

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <ApplicationServices/ApplicationServices.h>

#define MAX_SPLIT_PARTS     1024
#define MAX_FILENAME_CHARS  256

int usage()
{
    printf("usage: pdfsplit input.pdf splitPageNum1 splitPageNum2 ...\n\n"
           "  - input.pdf: the path to the input pdf file.\n\n"
           "  - splitPageNum1, ...: each one is a positive integer; the numbers\n"
           "    must not exceed the number of pages of the input file, and the\n"
           "    entire sequence must be strictly increasing.\n\n"
           "Example: splitPDF input.pdf 3 5\n\n"
           "This will split file input.pdf into 3 files (assuming input.pdf is 10\n"
           "pages long):\n\n"
           "  - input.1-3.pdf contains page 1-3;\n"
           "  - input.4-5.pdf contains page 4-5;\n"
           "  - input.6-10.pdf contains page 6-10.\n\n");
    return 1;
}

int collectPageNums(size_t splitPageNums[], int size, size_t max, char *argv[], int argc)
{
    int i;

    for (i = 0; i < size && i < argc; i++)
    {
        splitPageNums[i] = strtol(argv[i], NULL, 10);

        if (! splitPageNums[i] ||
            splitPageNums[i] < 1 ||
            splitPageNums[i] > max ||
            (i > 0 && splitPageNums[i] <= splitPageNums[i - 1]))
        {
            printf("Error: invalid split page number: %s\n", argv[i]);
            return 0;
        }
    }

    return i;
}

void getBaseFilename(char *baseFn, int size, const char *file)
{
    int i, j, len = strlen(file);

    for (i = len - 1; i > 0 && file[i] != '.'; i--)
        ;

    for (j = 0; j < i && j < size - 1; j++)
        baseFn[j] = file[j];

    baseFn[j] = '\0';
}

void writePages(CFURLRef url, CGPDFDocumentRef inputDoc,
                size_t start, size_t end)
{
    CGContextRef writeContext = NULL;
    CGRect mediaBox;
    CGPDFPageRef page;
    size_t i;

    for (i = start; i <= end; i++)
    {
        page = CGPDFDocumentGetPage(inputDoc, i);
        mediaBox = CGPDFPageGetBoxRect(page, kCGPDFMediaBox);

        if (! writeContext)
            writeContext = CGPDFContextCreateWithURL(url, &mediaBox, NULL);

        CGPDFContextBeginPage(writeContext, NULL);
        CGContextDrawPDFPage(writeContext, page);
        CGPDFContextEndPage(writeContext);
    }

    if (writeContext)
    {
        CGPDFContextClose(writeContext);
        CGContextRelease(writeContext);
    }
}

int main(int argc, char *argv[])
{
    int error = 0, i;
    const char *inputFn;
    char outputFn[MAX_FILENAME_CHARS], baseFn[MAX_FILENAME_CHARS];
    CGDataProviderRef inputData;
    CGPDFDocumentRef inputDoc;
    size_t maxPages, splitPageNums[MAX_SPLIT_PARTS], totalPageNums, startPageNum;

    if (argc < 3)
        return usage();

    inputFn = argv[1];
    inputData = CGDataProviderCreateWithFilename(inputFn);

    if (! inputData)
    {
        printf("Error: failed to open %s\n", inputFn);
        error = -1;
        goto cleanup;
    }

    inputDoc = CGPDFDocumentCreateWithProvider(inputData);

    if (! inputDoc)
    {
        printf("Error: failed to open %s\n", inputFn);
        error = -1;
        goto cleanup;
    }
    
    maxPages = CGPDFDocumentGetNumberOfPages(inputDoc);
    printf("%s has %lu pages\n", inputFn, maxPages);

    // Remove the first two arguments
    totalPageNums = collectPageNums(splitPageNums, MAX_SPLIT_PARTS, maxPages,
                                    argv + 2, argc - 2);

    if (! totalPageNums)
        goto cleanup;

    if (totalPageNums < MAX_SPLIT_PARTS &&
        splitPageNums[totalPageNums - 1] < maxPages)
        splitPageNums[totalPageNums++] = maxPages;

    getBaseFilename(baseFn, MAX_FILENAME_CHARS, basename((char *) inputFn));

    startPageNum = 1;
    for (i = 0; i < totalPageNums; i++)
    {
        CFURLRef url;

        if (startPageNum < splitPageNums[i])
            snprintf(outputFn, MAX_FILENAME_CHARS, "%s.%lu-%lu.pdf",
                     baseFn, startPageNum, splitPageNums[i]);
        else
            snprintf(outputFn, MAX_FILENAME_CHARS, "%s.%lu.pdf",
                     baseFn, startPageNum);

        url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *) outputFn,
                                                      strlen(outputFn), false);

        printf("Writing page %lu-%lu to %s...\n",
               startPageNum, splitPageNums[i], outputFn);

        writePages(url, inputDoc, startPageNum, splitPageNums[i]);
        CFRelease(url);

        startPageNum = splitPageNums[i] + 1;
    }

cleanup:
    CGPDFDocumentRelease(inputDoc);
    CGDataProviderRelease(inputData);

    return error;
}

