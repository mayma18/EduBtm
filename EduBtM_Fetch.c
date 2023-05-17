/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational-Purpose Object Storage System            */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Database and Multimedia Laboratory                                      */
/*                                                                            */
/*    Computer Science Department and                                         */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: kywhang@cs.kaist.ac.kr                                          */
/*    phone: +82-42-350-7722                                                  */
/*    fax: +82-42-350-8380                                                    */
/*                                                                            */
/*    Copyright (c) 1995-2013 by Kyu-Young Whang                              */
/*                                                                            */
/*    All rights reserved. No part of this software may be reproduced,        */
/*    stored in a retrieval system, or transmitted, in any form or by any     */
/*    means, electronic, mechanical, photocopying, recording, or otherwise,   */
/*    without prior written permission of the copyright owner.                */
/*                                                                            */
/******************************************************************************/
/*
 * Module: EduBtM_Fetch.c
 *
 * Description :
 *  Find the first object satisfying the given condition.
 *  If there is no such object, then return with 'flag' field of cursor set
 *  to CURSOR_EOS. If there is an object satisfying the condition, then cursor
 *  points to the object position in the B+ tree and the object identifier
 *  is returned via 'cursor' parameter.
 *  The condition is given with a key value and a comparison operator;
 *  the comparison operator is one among SM_BOF, SM_EOF, SM_EQ, SM_LT, SM_LE, SM_GT, SM_GE.
 *
 * Exports:
 *  Four EduBtM_Fetch(PageID*, KeyDesc*, KeyValue*, Four, KeyValue*, Four, BtreeCursor*)
 */


#include <string.h>
#include "EduBtM_common.h"
#include "BfM.h"
#include "EduBtM_Internal.h"


/*@ Internal Function Prototypes */
Four edubtm_Fetch(PageID*, KeyDesc*, KeyValue*, Four, KeyValue*, Four, BtreeCursor*);



/*@================================
 * EduBtM_Fetch()
 *================================*/
/*
 * Function: Four EduBtM_Fetch(PageID*, KeyDesc*, KeyVlaue*, Four, KeyValue*, Four, BtreeCursor*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  Find the first object satisfying the given condition. See above for detail.
 *
 * Returns:
 *  error code
 *    eBADPARAMETER_BTM
 *    some errors caused by function calls
 *
 * Side effects:
 *  cursor  : The found ObjectID and its position in the Btree Leaf
 *            (it may indicate a ObjectID in an  overflow page).
 */
Four EduBtM_Fetch(
    PageID   *root,		/* IN The current root of the subtree */
    KeyDesc  *kdesc,		/* IN Btree key descriptor */
    KeyValue *startKval,	/* IN key value of start condition */
    Four     startCompOp,	/* IN comparison operator of start condition */
    KeyValue *stopKval,		/* IN key value of stop condition */
    Four     stopCompOp,	/* IN comparison operator of stop condition */
    BtreeCursor *cursor)	/* OUT Btree Cursor */
{
    int i;
    Four e;		   /* error number */

    
    if (root == NULL) ERR(eBADPARAMETER_BTM);

    /* Error check whether using not supported functionality by EduBtM */
    for(i=0; i<kdesc->nparts; i++)
    {
        if(kdesc->kpart[i].type!=SM_INT && kdesc->kpart[i].type!=SM_VARSTRING)
            ERR(eNOTSUPPORTED_EDUBTM);
    }

    if (startCompOp == SM_BOF){
        e =edubtm_FirstObject(root, kdesc, stopKval, stopCompOp, cursor);
        if(e<0)ERR(e);
    }


    if (startCompOp == SM_EOF){
        e =edubtm_LastObject(root, kdesc, stopKval, stopCompOp, cursor);
        if(e<0)ERR(e);
    }


    else{
        e =edubtm_Fetch(root, kdesc, startKval, startCompOp, stopKval, stopCompOp, cursor);
        if(e<0)ERR(e);
    } 

    return(eNOERROR);

} /* EduBtM_Fetch() */



/*@================================
 * edubtm_Fetch()
 *================================*/
/*
 * Function: Four edubtm_Fetch(PageID*, KeyDesc*, KeyVlaue*, Four, KeyValue*, Four, BtreeCursor*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  Find the first object satisfying the given condition.
 *  This function handles only the following conditions:
 *  SM_EQ, SM_LT, SM_LE, SM_GT, SM_GE.
 *
 * Returns:
 *  Error code *   
 *    eBADCOMPOP_BTM
 *    eBADBTREEPAGE_BTM
 *    some errors caused by function calls
 */
Four edubtm_Fetch(
    PageID              *root,          /* IN The current root of the subtree */
    KeyDesc             *kdesc,         /* IN Btree key descriptor */
    KeyValue            *startKval,     /* IN key value of start condition */
    Four                startCompOp,    /* IN comparison operator of start condition */
    KeyValue            *stopKval,      /* IN key value of stop condition */
    Four                stopCompOp,     /* IN comparison operator of stop condition */
    BtreeCursor         *cursor)        /* OUT Btree Cursor */
{
    Four                e;              /* error number */
    Four                cmp;            /* result of comparison */
    Two                 idx;            /* index */
    PageID              child;          /* child page when the root is an internla page */
    Two                 alignedKlen;    /* aligned size of the key length */
    BtreePage           *apage;         /* a Page Pointer to the given root */
    BtreeOverflow       *opage;         /* a page pointer if it necessary to access an overflow page */
    Boolean             found;          /* search result */
    PageID              *leafPid;       /* leaf page pointed by the cursor */
    Two                 slotNo;         /* slot pointed by the slot */
    PageID              ovPid;          /* PageID of the overflow page */
    PageNo              ovPageNo;       /* PageNo of the overflow page */
    PageID              prevPid;        /* PageID of the previous page */
    PageID              nextPid;        /* PageID of the next page */
    ObjectID            *oidArray;      /* array of the ObjectIDs */
    Two                 iEntryOffset;   /* starting offset of an internal entry */
    btm_InternalEntry   *iEntry;        /* an internal entry */
    Two                 lEntryOffset;   /* starting offset of a leaf entry */
    btm_LeafEntry       *lEntry;        /* a leaf entry */


    /* Error check whether using not supported functionality by EduBtM */
    int i;
    for(i=0; i<kdesc->nparts; i++)
    {
        if(kdesc->kpart[i].type!=SM_INT && kdesc->kpart[i].type!=SM_VARSTRING)
            ERR(eNOTSUPPORTED_EDUBTM);
    }

    e = BfM_GetTrain(root, (char**)&apage, PAGE_BUF);
    if(e<0)ERR(e);


/*If the root page given as a parameter is an internal page, Call edubtm_Fetch() recursively*/    

    if (apage->any.hdr.type & INTERNAL) {
        found = edubtm_BinarySearchInternal(apage, kdesc, startKval, &idx);

    if (idx != -1) {
        iEntryOffset = apage->bi.slot[-idx];
        iEntry = (btm_InternalEntry*)&apage->bi.data[iEntryOffset];
        nextPid.pageNo = iEntry->spid;
        nextPid.volNo = root->volNo;
    } 
    else {
        nextPid.pageNo = apage->bi.hdr.p0;
        nextPid.volNo = root->volNo;
    }
    e = edubtm_Fetch(&nextPid, kdesc, startKval, startCompOp, stopKval, stopCompOp, cursor);
    if(e<0) ERR(e);
    e = BfM_FreeTrain(root, PAGE_BUF);
    if(e<0)ERR(e);
    return(eNOERROR);
    }

/*If the root page given as a parameter is a leaf page Many Cases to take into acount*/
    
    else if (apage->any.hdr.type & LEAF) {
        found = edubtm_BinarySearchLeaf(apage, kdesc, startKval, &idx);
        leafPid = root;

        if (startCompOp == SM_EQ){
            if (found) slotNo = idx;
            else{
                cursor->flag = CURSOR_EOS;
                e = BfM_FreeTrain(root, PAGE_BUF);
                if(e<0)ERR(e);
                return(eNOERROR);
            }
        }

        else if(startCompOp == SM_LT){
            if (found){
                // if the index entry is the first one of the page then we need to go to the previous page
                if (idx == 0){
                    prevPid.pageNo = apage->bl.hdr.prevPage;
                    prevPid.volNo = root->volNo;

                    e = BfM_FreeTrain(root, PAGE_BUF);
                    if(e<0)ERR(e);

                    if(prevPid.pageNo == NIL){
                        cursor->flag = CURSOR_EOS;
                        return(eNOERROR);
                    }
                    e = BfM_GetTrain(&prevPid, (char**)&apage, PAGE_BUF);
                    if(e<0)ERR(e);
                    idx = apage->bl.hdr.nSlots - 1;
                    slotNo = idx;
                    leafPid = &prevPid;
                }
                else{
                    slotNo = idx - 1;
                }
            }
            else {
                cursor->flag = CURSOR_EOS;
                e = BfM_FreeTrain(root, PAGE_BUF);
                if(e<0)ERR(e);
                return(eNOERROR);
            }            
        }

        else if(startCompOp == SM_LE){
            if (found) {
                slotNo = idx;
            } else {
                if (idx != -1) slotNo = idx; // meaning the key is smaller than every element
                else {
                    cursor->flag = CURSOR_EOS;
                    e = BfM_FreeTrain(root, PAGE_BUF);
                    if(e<0)ERR(e);
                    return(eNOERROR);
                }
            }
        }


        else if (startCompOp == SM_GT){
            if (found){
                // if the index entry is the last one of the page then we need to go to the next page
                if (idx == apage->bl.hdr.nSlots - 1){
                    nextPid.pageNo = apage->bl.hdr.nextPage;
                    nextPid.volNo = root->volNo;

                    e = BfM_FreeTrain(root, PAGE_BUF);
                    if(e<0)ERR(e);

                    if(nextPid.pageNo == NIL){
                        cursor->flag = CURSOR_EOS;
                        return(eNOERROR);
                    }
                    e = BfM_GetTrain(&nextPid, (char**)&apage, PAGE_BUF);
                    if(e<0)ERR(e);
                    idx = 0;
                    slotNo = idx;
                    leafPid = &nextPid;
                }
                else{
                    slotNo = idx + 1;
                }
            }
            else {
                if (idx == apage->bl.hdr.nSlots - 1) {
                    nextPid.volNo = root->volNo;
                    nextPid.pageNo = apage->bl.hdr.nextPage;
                    e = BfM_FreeTrain(root, PAGE_BUF);
                    if(e<0) ERR(e);

                    if (nextPid.pageNo == NIL) {
                        cursor->flag = CURSOR_EOS;
                        return(eNOERROR);
                    } 
                    e = BfM_GetTrain(&nextPid, &apage, PAGE_BUF);
                    if(e<0) ERR(e);
                    idx = 0;
                    slotNo = idx;
                    leafPid = &nextPid;
                } else {
                    slotNo = idx + 1;
                }
                
            }
        }

        else if (startCompOp == SM_GE){
            if (found) idx = idx;
            else if (idx == -1) idx = 0;
            else {
                if(idx != apage->bl.hdr.nSlots - 1) idx += 1;
                else {
                    nextPid.volNo = root->volNo;
                    nextPid.pageNo = apage->bl.hdr.nextPage;
                    e = BfM_FreeTrain(root, PAGE_BUF);
                    if(e<0) ERR(e);

                    if (nextPid.pageNo == NIL) {
                        cursor->flag = CURSOR_EOS;
                        return(eNOERROR);
                    } 
                    e = BfM_GetTrain(&nextPid, &apage, PAGE_BUF);
                    if(e<0) ERR(e);
                    idx = 0;
                    slotNo = idx;
                    leafPid = &nextPid;

                }
            }
        }

        lEntryOffset = apage->bl.slot[-slotNo];
        lEntry = (btm_LeafEntry*)&apage->bl.data[lEntryOffset];
        alignedKlen = ALIGNED_LENGTH(lEntry->klen);
        oidArray = &lEntry->kval[alignedKlen];

        //SETUP THE CURSOR VALUES
        cursor->flag = CURSOR_ON;
        cursor->leaf = *leafPid;
        cursor->slotNo = slotNo;
        cursor->oid = *oidArray;
        cursor->key.len = lEntry->klen;
        memcpy(cursor->key.val, lEntry->kval, lEntry->klen);
        //END SETUP CURSOR


        if (stopCompOp != SM_EOF && stopCompOp != SM_BOF) {

            cmp = edubtm_KeyCompare(kdesc, &cursor->key, stopKval);

            if (stopCompOp == SM_EQ) {
                if (cmp != EQUAL) cursor->flag = CURSOR_INVALID;
            }

            if (stopCompOp == SM_LT) {
                if (cmp != LESS) cursor->flag = CURSOR_INVALID;
            }

            if (stopCompOp == SM_LE) {
                if (cmp != LESS && cmp != EQUAL) cursor->flag = CURSOR_INVALID;
            }

            if (stopCompOp == SM_GT) {
                if (cmp != GREATER) cursor->flag = CURSOR_INVALID;
            }

            if (stopCompOp == SM_GE) {
                if (cmp != GREATER && cmp != EQUAL) cursor->flag = CURSOR_INVALID;
            }
        }

        e = BfM_FreeTrain(root, PAGE_BUF);
        if(e<0) ERR(e);
        return (eNOERROR);

    }
    else ERR(eBADBTREEPAGE_BTM);
    
    
} /* edubtm_Fetch() */

