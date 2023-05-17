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
 * Module: edubtm_root.c
 *
 * Description : 
 *  This file has two routines which are concerned with the changing the
 *  current root node. When an overflow or a underflow occurs in the root page
 *  the root node should be changed. But we don't change the root node to
 *  the new page. The old root page is used as the new root node; thus the
 *  root page is fixed always.
 *
 * Exports:
 *  Four edubtm_root_insert(ObjectID*, PageID*, InternalItem*)
 */


#include <string.h>
#include "EduBtM_common.h"
#include "Util.h"
#include "BfM.h"
#include "EduBtM_Internal.h"



/*@================================
 * edubtm_root_insert()
 *================================*/
/*
 * Function: Four edubtm_root_insert(ObjectID*, PageID*, InternalItem*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS BtM.
 *  For ODYSSEUS/EduCOSMOS EduBtM, refer to the EduBtM project manual.)
 *
 *  This routine is called when a new entry was inserted into the root page and
 *  it was splitted two pages, 'root' and 'item->pid'. The new root should be
 *  made by the given root Page IDentifier and the sibling entry 'item'.
 *  We make it a rule to fix the root page; so a new page is allocated and
 *  the root node is copied into the newly allocated page. The root node
 *  is changed so that it points to the newly allocated node and the 'item->pid'.
 *
 * Returns:
 *  Error code
 *    some errors caused by function calls
 */
Four edubtm_root_insert(
    ObjectID     *catObjForFile, /* IN catalog object of B+ tree file */
    PageID       *root,		 /* IN root Page IDentifier */
    InternalItem *item)		 /* IN Internal item which will be the unique entry of the new root */
{
    Four      e;		/* error number */
    PageID    newPid;		/* newly allocated page */
    PageID    nextPid;		/* PageID of the next page of root if root is leaf */
    BtreePage *rootPage;	/* pointer to a buffer holding the root page */
    BtreePage *newPage;		/* pointer to a buffer holding the new page */
    BtreeLeaf *nextPage;	/* pointer to a buffer holding next page of root */
    btm_InternalEntry *entry;	/* an internal entry */
    Boolean   isTmp;

    e = btm_AllocPage(catObjForFile, root, &newPid);
    if (e < 0) ERR(e);
    e = BfM_GetTrain(root, (char**)&rootPage, PAGE_BUF);
    if (e < 0) ERR( e ); 
    e = BfM_GetNewTrain(&newPid, (char**)&newPage, PAGE_BUF);
    if (e < 0) ERR( e );

    /* Copy the old root page into the page allocated.*/
    memcpy(newPage, rootPage, PAGESIZE);
    newPage->any.hdr.pid = newPid;
    /*Initialize the old root page as the new root page.*/
    e = edubtm_InitInternal(root, TRUE, isTmp);
    if(e<0) ERR(e);

    /*Make the page allocated and the page created by the split of the root page to be children pages of the new root page.*/
    /*Insert the internal index entry pointing to the page created by the split into the new root page.*/
    rootPage->bi.slot[0] = 0; // nothing in it except the item that we're going to insert 
    entry = &rootPage->bi.data[rootPage->bi.slot[0]];
    memcpy(entry, item, sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + item->klen));
    rootPage->bi.hdr.nSlots = 1;
    rootPage->bi.hdr.free = sizeof(ShortPageID) + ALIGNED_LENGTH(sizeof(Two) + item->klen);
    rootPage->bi.hdr.p0 = newPid.pageNo;
    /*If the children pages of the new root page are leaf pages, set the doubly linked list between two children pages.*/
    if(newPage->any.hdr.type & LEAF){
        nextPid.pageNo = newPage->bl.hdr.nextPage;
        nextPid.volNo = newPage->bl.hdr.pid.volNo;
        e = BfM_GetTrain(&nextPage, (char**)&nextPage, PAGE_BUF);
        if (e<0) ERR( e );
        nextPage->hdr.prevPage = newPid.pageNo;
        e = BfM_SetDirty(&newPage->bl.hdr.nextPage, PAGE_BUF);
        if (e < 0) ERR( e );
        e = BfM_FreeTrain(&newPage->bl.hdr.nextPage, PAGE_BUF);
        if (e < 0) ERR( e );
    }
    e = BfM_SetDirty(&newPid, PAGE_BUF);
    if(e<0)ERR( e );
    e = BfM_FreeTrain(&newPid, PAGE_BUF);
    if(e<0) ERR( e );
    e = BfM_SetDirty(root, PAGE_BUF);
    if(e<0)ERR( e );
    e = BfM_FreeTrain(root, PAGE_BUF);
    if(e<0) ERR( e );
    return(eNOERROR);
    
} /* edubtm_root_insert() */
