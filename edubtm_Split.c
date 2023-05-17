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
 * Module: edubtm_Split.c
 *
 * Description : 
 *  This file has three functions about 'split'.
 *  'edubtm_SplitInternal(...) and edubtm_SplitLeaf(...) insert the given item
 *  after spliting, and return 'ritem' which should be inserted into the
 *  parent page.
 *
 * Exports:
 *  Four edubtm_SplitInternal(ObjectID*, BtreeInternal*, Two, InternalItem*, InternalItem*)
 *  Four edubtm_SplitLeaf(ObjectID*, PageID*, BtreeLeaf*, Two, LeafItem*, InternalItem*)
 */


#include <string.h>
#include "EduBtM_common.h"
#include "BfM.h"
#include "EduBtM_Internal.h"



/*@================================
 * edubtm_SplitInternal()
 *================================*/
/*
 * Function: Four edubtm_SplitInternal(ObjectID*, BtreeInternal*,Two, InternalItem*, InternalItem*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  At first, the function edubtm_SplitInternal(...) allocates a new internal page
 *  and initialize it.  Secondly, all items in the given page and the given
 *  'item' are divided by halves and stored to the two pages.  By spliting,
 *  the new internal item should be inserted into their parent and the item will
 *  be returned by 'ritem'.
 *
 *  A temporary page is used because it is difficult to use the given page
 *  directly and the temporary page will be copied to the given page later.
 *
 * Returns:
 *  error code
 *    some errors caused by function calls
 *
 * Note:
 *  The caller should call BfM_SetDirty() for 'fpage'.
 */
Four edubtm_SplitInternal(
    ObjectID                    *catObjForFile,         /* IN catalog object of B+ tree file */
    BtreeInternal               *fpage,                 /* INOUT the page which will be splitted */
    Two                         high,                   /* IN slot No. for the given 'item' */
    InternalItem                *item,                  /* IN the item which will be inserted */
    InternalItem                *ritem)                 /* OUT the item which will be returned by spliting */
{
    Four                        e;                      /* error number */
    Two                         i;                      /* slot No. in the given page, fpage */
    Two                         j;                      /* slot No. in the splitted pages */
    Two                         k;                      /* slot No. in the new page */
    Two                         maxLoop;                /* # of max loops; # of slots in fpage + 1 */
    Four                        sum;                    /* the size of a filled area */
    Boolean                     flag=FALSE;             /* TRUE if 'item' become a member of fpage */
    PageID                      newPid;                 /* for a New Allocated Page */
    BtreeInternal               *npage;                 /* a page pointer for the new allocated page */
    Two                         fEntryOffset;           /* starting offset of an entry in fpage */
    Two                         nEntryOffset;           /* starting offset of an entry in npage */
    Two                         entryLen;               /* length of an entry */
    btm_InternalEntry           *fEntry;                /* internal entry in the given page, fpage */
    btm_InternalEntry           *nEntry;                /* internal entry in the new page, npage*/
    Boolean                     isTmp;

    isTmp = FALSE;
    e = btm_AllocPage(catObjForFile, &fpage->hdr.pid, &newPid);
    if(e<0)ERR(e);

    e = edubtm_InitInternal(&newPid, FALSE, isTmp);
    if(e<0)ERR(e);

    maxLoop = fpage->hdr.nSlots + 1;
    sum = 0;
    i = 0;
    j = 0; 

    for(j = 0; (j < maxLoop && sum < BI_HALF) ; ++j){
        if (j == high + 1) {
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + item->klen);
            isTmp = TRUE;
        }
        else {
            fEntryOffset = fpage->slot[-i];
            fEntry = &fpage->data[fEntryOffset];
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(2)+ fEntry->klen); 
            ++i;
        }
        sum += entryLen + sizeof(Two);
    }
    fpage->hdr.nSlots = j;

    if(fpage->hdr.type & ROOT){
        fpage->hdr.type ^= ROOT;
    }

    if (i == high + 1) {
        ritem->spid = newPid.pageNo;
        ritem->klen = item->klen;
        memcpy(ritem->kval, item->kval, item->klen);
    } 
    else {
        fEntry = (btm_InternalEntry*)&(fpage->data[fpage->slot[-i]]);
        ritem->spid = newPid.pageNo;
        ritem->klen = fEntry->klen;
        memcpy(ritem->kval, fEntry->kval, fEntry->klen);
        ++i;
    }


    e = BfM_GetNewTrain(&newPid, (char**)&npage, PAGE_BUF);
    if (e<0)ERR(e);

    fEntry = (btm_InternalEntry*)&(fpage->data[fpage->slot[-i]]); 
    npage->hdr.p0 = fEntry->spid; 
    k = 0;
    // j+1 because we add one more entry just before
    for (j = j+1; j < maxLoop; ++j) {
        if (i == high + 1) {
            isTmp = FALSE;
            npage->slot[-k] = nEntryOffset;
            nEntry = (btm_InternalEntry*)&npage->data[npage->slot[-k]];
            nEntry->klen = item->klen;
            nEntry->spid = item->spid;
            memcpy(nEntry->kval, item->kval, item->klen);
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + nEntry->klen);
        }
        else {
            fEntry = (btm_InternalEntry*)&(fpage->data[fpage->slot[-i]]);
            entryLen = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + fEntry->klen);
            npage->slot[-k] = nEntryOffset;
            memcpy(&npage->data[npage->slot[-k]], fEntry, entryLen);
            ++i;
        }
        nEntryOffset += entryLen;
        ++k;
    }

    if (isTmp){
        if (sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + item->klen) > BI_CFREE(fpage)) {
            edubtm_CompactInternalPage(fpage, NIL);
        }
        for(j = fpage->hdr.nSlots - 1; high + 1 <= j; --j){
            fpage->slot[-j-1] = fpage->slot[-j];
        }
        fpage->slot[-high-1] = fpage->hdr.free;

        fEntry = &fpage->data[fpage->hdr.free];

        memcpy(fEntry, item, sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + item->klen));

        fpage->hdr.free += sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + item->klen);
        fpage->hdr.nSlots += 1;
    }
    e = BfM_SetDirty(&newPid, PAGE_BUF);
    if(e<0)ERR(e);

    e = BfM_FreeTrain(&newPid, PAGE_BUF);
    if(e<0)ERR(e);

    
    return(eNOERROR);
        

    
} /* edubtm_SplitInternal() */



/*@================================
 * edubtm_SplitLeaf()
 *================================*/
/*
 * Function: Four edubtm_SplitLeaf(ObjectID*, PageID*, BtreeLeaf*, Two, LeafItem*, InternalItem*)
 *
 * Description: 
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  The function edubtm_SplitLeaf(...) is similar to edubtm_SplitInternal(...) except
 *  that the entry of a leaf differs from the entry of an internal and the first
 *  key value of a new page is used to make an internal item of their parent.
 *  Internal pages do not maintain the linked list, but leaves do it, so links
 *  are properly updated.
 *
 * Returns:
 *  Error code
 *  eDUPLICATEDOBJECTID_BTM
 *    some errors caused by function calls
 *
 * Note:
 *  The caller should call BfM_SetDirty() for 'fpage'.
 */
Four edubtm_SplitLeaf(
    ObjectID                    *catObjForFile, /* IN catalog object of B+ tree file */
    PageID                      *root,          /* IN PageID for the given page, 'fpage' */
    BtreeLeaf                   *fpage,         /* INOUT the page which will be splitted */
    Two                         high,           /* IN slotNo for the given 'item' */
    LeafItem                    *item,          /* IN the item which will be inserted */
    InternalItem                *ritem)         /* OUT the item which will be returned by spliting */
{
    Four                        e;              /* error number */
    Two                         i;              /* slot No. in the given page, fpage */
    Two                         j;              /* slot No. in the splitted pages */
    Two                         k;              /* slot No. in the new page */
    Two                         maxLoop;        /* # of max loops; # of slots in fpage + 1 */
    Four                        sum;            /* the size of a filled area */
    PageID                      newPid;         /* for a New Allocated Page */
    PageID                      nextPid;        /* for maintaining doubly linked list */
    BtreeLeaf                   tpage;          /* a temporary page for the given page */
    BtreeLeaf                   *npage;         /* a page pointer for the new page */
    BtreeLeaf                   *mpage;         /* for doubly linked list */
    btm_LeafEntry               *itemEntry;     /* entry for the given 'item' */
    btm_LeafEntry               *fEntry;        /* an entry in the given page, 'fpage' */
    btm_LeafEntry               *nEntry;        /* an entry in the new page, 'npage' */
    ObjectID                    *iOidArray;     /* ObjectID array of 'itemEntry' */
    ObjectID                    *fOidArray;     /* ObjectID array of 'fEntry' */
    Two                         fEntryOffset;   /* starting offset of 'fEntry' */
    Two                         nEntryOffset;   /* starting offset of 'nEntry' */
    Two                         oidArrayNo;     /* element No in an ObjectID array */
    Two                         alignedKlen;    /* aligned length of the key length */
    Two                         itemEntryLen;   /* length of entry for item */
    Two                         entryLen;       /* entry length */
    Boolean                     flag;
    Boolean                     isTmp;

/*-----------------------------------------------Autre maniere de faire, utiliser la page temporaire 
pour store tous les valeurs que doivent 
prendre splitted page, comme ca pas besoin de faire un traitement apres pour item a ajouté*/

    /*Allocate a new page & Initialize the allocated page as a leaf page.*/
    e =btm_AllocPage(catObjForFile, root, &newPid);
    if(e<0) ERR(e);
    e=edubtm_InitLeaf(&newPid, FALSE, FALSE);
    if(e<0) ERR(e);

    flag = FALSE;
    maxLoop = fpage->hdr.nSlots + 1;
    sum = 0;
    i = 0;
    j = 0; 
    itemEntryLen = BTM_LEAFENTRY_FIXED + ALIGNED_LENGTH(item->klen) + sizeof(ObjectID);

    for (j = 0; (j<maxLoop && sum < BL_HALF); ++j){
        if (j == high + 1){
            flag = TRUE;
            entryLen = itemEntryLen; // in order to update sum
        }
        else {
            fEntryOffset = fpage->slot[-i];
            fEntry = (btm_LeafEntry*)&(fpage->data[fEntryOffset]);
            entryLen = BTM_LEAFENTRY_FIXED + ALIGNED_LENGTH(fEntry->klen) + sizeof(ObjectID); //in order to update sum
            ++i;
        }
        sum += entryLen + 2; // 2 is for slot size
    }
    fpage->hdr.nSlots = i;

    // ATTENTION A SAVOIR SI ON RAJOUTE OU PAS ---------------------------------------------------
    if(fpage->hdr.type & ROOT){
        fpage->hdr.type ^= ROOT;
    }
    // FIN DU ATTENTION -----------------------------------------------------------------------------
    // now we work on the new page, with the index entry remaining
    e = BfM_GetTrain(&newPid, &npage, PAGE_BUF);
    if(e<0)ERR(e);
    k = 0; 

    for(j = j ; j <maxLoop; ++j){
        // set up info for the new index entry
        nEntryOffset = npage->hdr.free;
        npage->slot[-k] = nEntryOffset;
        nEntry = &npage->data[nEntryOffset];


        if(j == high + 1){
            flag = FALSE; // no need to do shift any things at the end 
            itemEntry = nEntry;
            // we can add the object now: 
            itemEntry->klen = item->klen;
            itemEntry->nObjects = item->nObjects;
            memcpy(itemEntry->kval, item->kval, item->klen);
            iOidArray = &itemEntry->kval[alignedKlen];
            *iOidArray = item->oid;

            entryLen = itemEntryLen;
        }
        else {
            // we also add the entry index now : 
            fEntryOffset = fpage->slot[-i];
            fEntry = &fpage->data[fEntryOffset];

            entryLen = (BTM_LEAFENTRY_FIXED + ALIGNED_LENGTH(fEntry->klen) + sizeof(ObjectID)); 
            memcpy(nEntry, fEntry, entryLen);
            
            if(fEntryOffset + entryLen == fpage->hdr.free){
                fpage->hdr.free -= entryLen;
            } else{
                fpage->hdr.unused += entryLen;  
            }  
            ++i;
        }

        ++k;
        npage->hdr.free += entryLen;
    }

    npage->hdr.nSlots = k;

    // we still need to add item, if He needs to go in the splitted page 

    if(flag){
        if(itemEntryLen > BL_CFREE(fpage)){
            edubtm_CompactLeafPage(fpage, NIL);
        }
        // shift every element, in order to have to have one free slot in arrayslot, but we store the index entry at the end of Data area 
        for(i = fpage->hdr.nSlots - 1; high + 1 <= i; --i){
            fpage->slot[-i-1] = fpage->slot[-i];
        }
        fpage->slot[-high-1] = fpage->hdr.free;
        fEntry = &fpage->data[fpage->slot[-high-1]];
        itemEntry = fEntry;

        itemEntry->klen = item->klen;
        itemEntry->nObjects = item->nObjects;
        memcpy(itemEntry->kval, item->kval, item->klen);
        iOidArray = &itemEntry->kval[alignedKlen];
        *iOidArray = item->oid;

        fpage->hdr.free += itemEntryLen;
        fpage->hdr.nSlots += 1;
    }


    // we need to update the doubly linked list 
    // the ritem returned, is the first of the newpage 
    nEntry = &npage->data[npage->slot[0]];
    ritem->spid = newPid.pageNo;
    item->klen = nEntry->klen;
    memcpy(ritem->kval, nEntry->kval, nEntry->klen);
    e = BfM_SetDirty(&newPid, PAGE_BUF);
    if(e<0) ERR(e);
    e = BfM_FreeTrain(&newPid, PAGE_BUF);
    if(e<0) ERR(e);




    return(eNOERROR);
    
} /* edubtm_SplitLeaf() */
