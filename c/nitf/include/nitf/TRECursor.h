/* =========================================================================
 * This file is part of NITRO
 * =========================================================================
 *
 * (C) Copyright 2004 - 2008, General Dynamics - Advanced Information Systems
 *
 * NITRO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, If not,
 * see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __NITF_TRE_CURSOR_H__
#define __NITF_TRE_CURSOR_H__

#include "nitf/IntStack.h"
#include "nitf/TRE.h"
#include "nitf/TREDescription.h"
#include "nitf/Utils.h"

NITF_CXX_GUARD
/*!
 *  A structure for keeping track of the
 *  state of the TRE Description when processing it
 *
 *  This is currently public, but it may be a good idea
 *  to move everything related to the nitf_TRECursor
 *  out of the public realm.
 */
typedef struct _nitf_TRECursor
{
    /* DO NOT TOUCH THESE! */
    int numItems;               /* holds the number of descPtr items */
    int index;                  /* holds the current index to process */
    int looping;                /* keeps track of how deep we are in the loops */
    nitf_IntStack *loop;        /* holds loopcount for each level of loops */
    nitf_IntStack *loop_idx;    /* holds the indexes for each level of loops (arrays) */
    nitf_IntStack *loop_rtn;    /* holds the endloop bookmark for each level of loops */
    nitf_TRE *tre;              /* the TRE associated with this cursor */
    nitf_TREDescription *end_ptr; /* holds a pointer to the end description */

    /* YOU CAN REFER TO THE MEMBERS BELOW IN YOUR CODE */
    nitf_TREDescription *prev_ptr; /* holds the previous description */
    nitf_TREDescription *desc_ptr;      /* pointer to the current nitf_TREDescription */
    char tag_str[256];          /* holds the fully qualified tag name of the current tag */
    int length;                 /* the length of the field
                                     * This should be used over the TREDescription length because
                                     * the field may need to be computed. This will contain the
                                     * actual, possibly computed length. */
}
nitf_TRECursor;





NITFAPI(nitf_Pair *) nitf_TRECursor_getTREPair(nitf_TRE * tre,
                                               char *descTag,
                                               char idx_str[10][10],
                                               int looping,
                                               nitf_Error * error);



NITFAPI(int) nitf_TRECursor_evalIf(nitf_TRE * tre,
                                   nitf_TREDescription * desc_ptr,
                                   char idx_str[10][10],
                                   int looping,
                                   nitf_Error * error);

/**
 * Helper function for evaluating loops
 * Returns the number of loops that will be processed
 */
NITFAPI(int) nitf_TRECursor_evalLoops(nitf_TRE * tre,
                                      nitf_TREDescription * desc_ptr,
                                      char idx_str[10][10],
                                      int looping,
                                      nitf_Error * error);


NITFAPI(int) nitf_TRECursor_evalCondLength(nitf_TRE * tre,
                                           nitf_TREDescription * desc_ptr,
                                           char idx_str[10][10],
                                           int looping,
                                           nitf_Error * error);

/*!
 *  Initializes the cursor
 *
 *  \param tre The input TRE
 *  \return A cursor for the TRE
 */
NITFAPI(nitf_TRECursor) nitf_TRECursor_begin(nitf_TRE * tre);


/*!
 *  Returns NITF_BOOL value noting if the cursor has reached the end
 *
 *  \param tre_cursor The TRECursor
 *  \return 1 if done, 0 otherwise
 */
NITFAPI(NITF_BOOL) nitf_TRECursor_isDone(nitf_TRECursor * tre_cursor);


/*!
 *  Cleans up any allocated members of the TRECursor
 *  This should be called when finished with an cursor
 *
 *  \param tre_cursor The TRECursor
 *  \return 1 if done, 0 otherwise
 */
NITFAPI(void) nitf_TRECursor_cleanup(nitf_TRECursor * tre_cursor);

/*!
 *  Duplicates the input cursor. This can be useful if you want to capture a
 *  snapshot of the cursor to revert back to.
 *
 *  \param tre The input TRE cursor
 *  \return A duplicated cursor for the TRE
 */
NITFAPI(nitf_TRECursor) nitf_TRECursor_clone(nitf_TRECursor *tre_cursor,
        nitf_Error * error);


/*!
 *  Iterate to the next tag in the TRE.
 *
 *  \param tre_cursor The cursor to use
 *  \param error The error to populate on failure
 *  \return NITF_SUCCESS on sucess or NITF_FAILURE otherwise
 */
NITFAPI(int) nitf_TRECursor_iterate(nitf_TRECursor * tre_cursor,
                              nitf_Error * error);


/*!
 *  Evaluates the given postfix expression, looking up fields in the TRE, or
 *  using constant integers.
 *
 *  \param tre      The TRE to use
 *  \param idx      The loop index/values
 *  \param looping  The current loop level
 *  \param expression The postfix expression
 *  \param error The error to populate on failure
 *  \return NITF_SUCCESS on sucess or NITF_FAILURE otherwise
 */
NITFAPI(int) nitf_TRECursor_evaluatePostfix(
        nitf_TRE *tre,
        char idx[10][10],
        int looping,
        char *expression,
        nitf_Error *error);

typedef unsigned int (*NITF_TRE_CURSOR_COUNT_FUNCTION) (nitf_TRE *,
                                                        char idx[10][10],
                                                        int,
                                                        nitf_Error*);


NITF_CXX_ENDGUARD

#endif

