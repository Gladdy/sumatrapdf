#include "PdfSearch.h"
#include <shlwapi.h>

PdfSearch::PdfSearch(PdfEngine *engine)
{
    tracker = NULL;
    text = NULL;
    pageText = NULL;
    coords = NULL;
    sensitive = false;
    forward = true;
    result.page = 1;
    result.len = 0;
    result.rects = NULL;
    this->engine = engine;
}

PdfSearch::~PdfSearch()
{
    Clear();
    free(result.rects);
}

void PdfSearch::Reset()
{
    if (pageText || coords) {
        free(pageText);
        pageText = NULL;
        free(coords);
        coords = NULL;
    }
}

void PdfSearch::SetText(TCHAR *text)
{
    this->Clear();
    this->text = tstr_dup(text);
    this->length = lstrlen(text);
}

void PdfSearch::SetDirection(bool forward)
{
    if (forward == this->forward)
        return;
    this->forward = forward;
    findIndex += length * (forward ? 1 : -1);
}

void PdfSearch::FillResultRects(TCHAR *found)
{
    fz_irect *c = &coords[found - pageText], *end = c + length;
    for (; c < end; c++) {
        // skip line breaks
        if (!c->x0 && !c->x1)
            continue;

        result.rects = (RECT *)realloc(result.rects, sizeof(RECT) * ++result.len);
        RECT *rc = &result.rects[result.len - 1];

        rc->left = c->x0;
        rc->top = c->y0;

        for (; c < end && (c->x0 || c->x1); c++);
        c--;

        rc->right = c->x1;
        rc->bottom = c->y1;
        if ((c[1].x0 || c[1].x1) && rc->right > c[1].x0)
            rc->right = c[1].x0;

        if (rc->bottom < rc->top)
            swap_int((int *)&rc->bottom, (int *)&rc->top);
        if (rc->right < rc->left)
            swap_int((int *)&rc->right, (int *)&rc->left);
    }
}

// TODO: use Boyer-Moore algorithm here (if it proves to be faster)
bool PdfSearch::FindTextInPage(int pageNo)
{
    if (!text)
        return false;
    if (!pageNo)
        pageNo = result.page;

    result.len = 0;
    free(result.rects);
    result.rects = NULL;
    result.page = pageNo;

    TCHAR *found = NULL;
    if (forward)
        found = (sensitive ? StrStr : StrStrI)(pageText + findIndex, text);
    else
        do { // unfortunately, there's no StrRStr...
            found = StrRStrI(pageText, pageText + findIndex, text);
            findIndex = found - pageText + length - 1;
        } while (found && sensitive && StrCmpN(text, found, length) != 0);

    if (found) {
        findIndex = found - pageText;
        FillResultRects(found);
        if (forward)
            findIndex += length;
    }

    return found != NULL;
}

bool PdfSearch::FindStartingAtPage(int pageNo)
{
    if (!text)
        return false;

    int total = engine->pageCount();
    while (1 <= pageNo && pageNo <= total && UpdateTracker(pageNo, total)) {
        Reset();

        pageText = engine->ExtractPageText(pageNo, _T(" "), &coords);
        findIndex = forward ? 0 : lstrlen(pageText);

        if (pageText && FindTextInPage(pageNo))
            return true;

        pageNo += forward ? 1 : -1;
    }
    
    // allow for the first/last page to be included in the next search
    result.page = forward ? total + 1 : 0;

    return false;
}

bool PdfSearch::FindFirst(int page, TCHAR *text)
{
    SetText(text);

    return FindStartingAtPage(page);
}

bool PdfSearch::FindNext()
{
    if (FindTextInPage())
        return true;

    int newPage = result.page + (forward ? 1 : -1);
    return FindStartingAtPage(newPage);
}
