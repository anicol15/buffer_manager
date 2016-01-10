/*****************************************************************************/
/*************** Implementation of the Buffer Manager Layer ******************/
/*****************************************************************************/
#include <iostream>
#include "buf.h"
using namespace std;

/*
 * Reurns the id of the first page int the list that has (pin_counter==0)
 */
int returnsFirstPage(list<int> list1, DESCRIPTIONFRAME *bufDescr, int numbuf_global)
{
    int id_remove_page = -1;
    list<int>::iterator i;
    
    //traverse the list from the first element untill the last one
    for (i = list1.begin(); i != list1.end(); ++i)
    {
        for (int k = 0; k < numbuf_global; k++) //find the page with pageNo = *i in the bufDescr
        {
            if (bufDescr[k].pageNo == *i)
            {
                if (bufDescr[k].pin_count == 0)
                {
                    id_remove_page = *i;
                    return id_remove_page;
                }
            }
        } //for inner
    } //for outer
    return id_remove_page;
}

/*
 * Reurns the id of the last page that was hated from the love list
 */
int removeHate(list<int> hateList, DESCRIPTIONFRAME *bufDescr, int numbuf_global)
{
    int PageIdRemoved = -1;
    //remove from hate list if is not empty
    if( hateList.size()!=0)
    {
        //get the most recently page that inserted in the hate queue
        PageIdRemoved = returnsFirstPage(hateList, bufDescr, numbuf_global); //remove from hate list
    }
    return PageIdRemoved;
}

/*
 * Reurns the id of the first page that was loved from the love list
 */
int removeLove(list<int> loveList, DESCRIPTIONFRAME *bufDescr, int numbuf_global)
{
    int PageIdRemoved = -1;
    //remove from hate list if is not empty
    if (loveList.size() != 0)
    {
        //get the least recently page that inserted in the love queue
        PageIdRemoved = returnsFirstPage(loveList, bufDescr, numbuf_global); //remove from love list
    }
    return PageIdRemoved;
}

/*
 *  Returns true if the pageid exists in the given list
 */
bool lookuplist(list<int> l , PageId pageNo)
{
    list<int>::iterator i;
    for(i=l.begin(); i != l.end(); ++i){
        if(*i==pageNo){
            return TRUE;
        }
    }
    return FALSE; //not found the page with pageNo in the input list
}



// Define buffer manager error messages here
//enum bufErrCodes  {...};

// Define error message here
static const char* bufErrMsgs[] =
{
    // error message strings go here
    "error while writting the page in disk",
    "error while reading the page from disk",
    "error while allocating a frame in buffer pool",
    "error while deallocating a frame in buffer pool",
    "error while pinning a frame in buffer pool",
    "error while freeing the page",
    "error while freeing the page because the frame is in use",
    "error while searching for an unused page because there is not an any unused pages",
    "error trying to flush a page that is not exist in bufPool",
    "error trying to unpin a page that is not in use",
};

// Create a static "error_string_table" object and register the error messages
// with minibase system
static error_string_table bufTable(BUFMGR,bufErrMsgs);

BufMgr::BufMgr (int numbuf, Replacer *replacer) {
    this->numbuf_global = numbuf;  //initialize the global variable
    bufPool = new Page[numbuf];
    bufDescr = new Frame_Descriptor[numbuf];
    
    //initialize the table bufDescr
    for(int i=0;i<numbuf;i++)
    {
        bufDescr[i].dirty = 0;
        bufDescr[i].pageNo = -1;
        bufDescr[i].pin_count = 0;
    }
}

Status BufMgr::pinPage(PageId PageId_in_a_DB, Page*& page, int emptyPage)
{
    int i;
    // search for the page in bufDescr
    for(i=0; i< MINIBASE_BM->numbuf_global; i++)
    {
        if(PageId_in_a_DB == bufDescr[i].pageNo)
        {
            ++(bufDescr[i].pin_count);
            
            //bufDescr[i].pin_count = bufDescr[i].pin_count+1; //increment pin_count
            
            //if the page has changed, write it in the disk and initialize the dirty = 0 again
            if ( bufDescr[i].dirty == 1 )
            {
                Status write_status = MINIBASE_DB->write_page(PageId_in_a_DB, &bufPool[i]);
                if ( write_status != OK )
                {
                    return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_WRITTING_PAGE);
                }
            }
            
            bufDescr[i].dirty = 0;
            page = &bufPool[i];
            return OK;
        }
    }
    //end search for the page in bufDescr
    
    //if the page is NOT in bufDescr, Check for free frames in bufDescr
    
    for(int i=0; i < MINIBASE_BM->numbuf_global; i++)
    {
        if ( bufDescr[i].pageNo == -1 ) //there is a free page in bufDescr
        {
            bufDescr[i].pageNo = PageId_in_a_DB; //insert the new page in the free page
            //read the page and pin it.
            page = &bufPool[i];
            Status read_status = MINIBASE_DB->read_page(PageId_in_a_DB, page);
            if ( read_status != OK )
            {
                return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_READING_PAGE);
            }
            
            // update the page info
            ++(bufDescr[i].pin_count);
            bufDescr[i].dirty = 0;
            return OK;
        }
    }
    //end Check for free frames in bufDescr
    
    //remove a page from buffpool
    int PageIdRemoved = -1;
    //check if exists a page in hateList that is not used (pin_count == 0)
    PageIdRemoved = removeHate(hateList, bufDescr, MINIBASE_BM->numbuf_global);
    if(PageIdRemoved!=-1)  //exists a page in hateList that is not used (pin_count == 0)
    {
        hateList.remove(PageIdRemoved); //remove the page from the list
    }
    else //not exists a page in hateList that is not used (pin_count == 0)
    {
        //check if exists a page in loveList that is not used (pin_count == 0)
        PageIdRemoved = removeLove(loveList, bufDescr, MINIBASE_BM->numbuf_global);
        if(PageIdRemoved!=-1)
        {
            loveList.remove(PageIdRemoved); //remove the page from the list
        }
    }
    //
    if(PageIdRemoved != -1)
    {
        for (int i = 0; i < MINIBASE_BM->numbuf_global; i++) //all the pages in bufDescr
        {
            if (bufDescr[i].pageNo == PageIdRemoved)
            {
                if (bufDescr[i].dirty == 1) //the page has unsaved changes
                {
                    Status write_status = MINIBASE_DB->write_page(PageIdRemoved, &bufPool[i]);
                    if (write_status != OK)
                    {
                        return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_WRITTING_PAGE);
                    }
                }
                
                bufDescr[i].pageNo = PageId_in_a_DB; //insert the pinned frame in buffpool
                page = &bufPool[i];
                
                //read the page and pin it.
                Status read_status = MINIBASE_DB->read_page(PageId_in_a_DB, page);
                if ( read_status != OK )
                {
                    return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_READING_PAGE);
                }
                ++(bufDescr[i].pin_count);
                bufDescr[i].dirty = 0;
                return OK;
            }
        }
        
    }
    else
    {
        return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_NOT_EXIST_UNUSED_PAGE);
    }
    //end remove a page from buffpool
    return OK;
}//end pinPage


Status BufMgr::newPage(PageId& firstPageId, Page*& firstpage, int howmany)
{
    Status allocate_status = MINIBASE_DB->allocate_page(firstPageId, howmany);
    if(allocate_status != OK)
    {
        return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_ALLOCATING_PAGE);
    }
    Status pinPage_status = BufMgr::pinPage(firstPageId, firstpage, 0);
    if(pinPage_status != OK)
    {
        MINIBASE_FIRST_ERROR(BUFMGR, ERROR_PINNING_PAGE);
    }
    return OK;
    
}

Status BufMgr::flushPage(PageId pageid)
{
  	 int i;
    for(i=0; i< MINIBASE_BM->numbuf_global; i++) //outer for
    {
        if(pageid == bufDescr[i].pageNo) //outer if
        {
            if(bufDescr[i].dirty==1) //the page has changess
            {
                Status write_status = MINIBASE_DB->write_page(pageid, &bufPool[i]);
                if( write_status!= OK )
                {
                    return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_WRITTING_PAGE);
                }
            }
            return OK;
        }
    }
    return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_PAGE_NOT_EXIST);
}



//*************************************************************
//** This is the implementation of ~BufMgr
//************************************************************
BufMgr::~BufMgr()
{
    //flush All dirty pages
    Status flushAll_status = BufMgr::flushAllPages();
    //clear the hateList, loveList, bufPool and bufDescr
    hateList.clear();
    loveList.clear();
    
    delete[] bufPool;
    delete[] bufDescr;
    
}


//*************************************************************
//** This is the implementation of unpinPage
//************************************************************

Status BufMgr::unpinPage(PageId page_num, int dirty=FALSE, int hate = FALSE){
    int i;
    for(i=0; i< MINIBASE_BM->numbuf_global; i++) //outer for
    {
        if(page_num == bufDescr[i].pageNo) //outer if
        {
            //the page is in buffer pool
            if(dirty == 1) //there are changes in the page
            {
                Status write_status = MINIBASE_DB->write_page(page_num, &bufPool[i]);
                if ( write_status != OK )
                {
                    return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_WRITTING_PAGE);
                }
                
                bufDescr[i].dirty = 1;
            }
            
            if(bufDescr[i].pin_count >=1)
            {
                --(bufDescr[i].pin_count);
                if(lookuplist(loveList,bufDescr[i].pageNo) == 1) //check if the page is already in the love list, then ignore the new input for the page
                {
                    return OK;
                }
                if(hate==FALSE) //the user loves the page
                {
                    loveList.push_back(bufDescr[i].pageNo);  ////insert page in loveList - the new pages are inserted last
                    if(lookuplist(hateList,bufDescr[i].pageNo) == 1)
                    {
                        hateList.remove(bufDescr[i].pageNo);
                    }
                    return OK;
                }
                else //the user hates the page
                {
                    if(lookuplist(hateList,bufDescr[i].pageNo) == 0) //if the page is in the hatelist
                    {
                        hateList.push_front(bufDescr[i].pageNo); //insert page in hateList - the new pages are inserted first
                        return OK;
                    }
                }
                
            } //end bufDescr[i].pin_count >=1
            else
            {
                MINIBASE_FIRST_ERROR(BUFMGR, ERROR_PIN_COUNT_ZERO);
            }
        } //outer if
        
    } //outer for
    return OK;
}

//*************************************************************
//** This is the implementation of freePage
//************************************************************

Status BufMgr::freePage(PageId globalPageId)
{
  	 int i;
    for(i=0; i< MINIBASE_BM->numbuf_global; i++) //outer for
    {
        if(globalPageId == bufDescr[i].pageNo) //outer if
        {
            if (bufDescr[i].pin_count <= 0) //the page is not in use
            {
                if(bufDescr[i].dirty==1) //the page has changess
                {
                    Status write_status = MINIBASE_DB->write_page(globalPageId, &bufPool[i]);
                    if( write_status != OK )
                    {
                        return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_WRITTING_PAGE);
                    }
                    
                }
                //initialize the page
                bufDescr[i].pageNo = -1;
                bufDescr[i].pin_count = 0;
                bufDescr[i].dirty = 0;
                
                //remove the page from hateList if exists
                if(lookuplist(hateList, globalPageId) != 0)
                {
                    hateList.remove(globalPageId);
                }
                //remove the page from loveList if exists
                if(lookuplist(loveList, globalPageId) != 0)
                {
                    loveList.remove(globalPageId);
                }
                //deallocate the page
                Status deallocate_page_status=MINIBASE_DB->deallocate_page(globalPageId, 1);
                
                if( OK != deallocate_page_status )
                {
                    return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_DEALLOCATING_PAGE);
                }
                
                return OK;
                
            }
            else //the page is in use
            {
                return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_FREEING_PAGE_IN_USE);
            }
        }
    }
    return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_FREEING_PAGE);
}

Status BufMgr::flushAllPages()
{
  	 int i;
    for(i=0; i< MINIBASE_BM->numbuf_global; i++) //outer for
    {
        if(bufDescr[i].dirty==1) //the page has changess
        {
            Status write_status = MINIBASE_DB->write_page(bufDescr[i].pageNo, &bufPool[i]);
            if( write_status != OK )
            {
                return MINIBASE_FIRST_ERROR(BUFMGR, ERROR_WRITTING_PAGE);
            }
        }					
    }
    return OK;
}