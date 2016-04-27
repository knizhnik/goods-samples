/////////////////////////////////////////////////////////////
//  header file GoodsTLCache.h

#ifndef GOODSLOCALTC_H
#define GOODSLOCALTC_H

//To implement thread local transactions we need 
//to add this to lines at the begin and and of the
//thread procedure
#ifdef NO_USE_GOODS_TL_CACHE

#define BEGIN_GOODS_CACHE										
#define END_GOODS_CACHE			

#else

#define BEGIN_GOODS_CACHE		cache_manager mng;\
								mng.attach();

#define END_GOODS_CACHE			mng.detach();

#endif //USE_GOODS_TL_CACHE


#endif //GOODSLOCALTC_H