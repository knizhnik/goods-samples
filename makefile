DEBUG?=1

CC = g++ -std=c++11
#CC = g++
SYSDEFS = -pthread -DGOODS_SUPPORT_EXCEPTIONS -DDEBUG_LEVEL=DEBUG_TRACE -DPTHREADS -DPGSQL_ORM=1

ifeq ($(DEBUG), 1)
OPTLEVEL = 0
else
OPTLEVEL = 3
endif
CCFLAGS = -c -g -O$(OPTLEVEL) $(SYSDEFS) -I. -I../goods/inc -I../goods/src -Iinc -Igoods-diff -Wall
LD = g++
LDFLAGS = $(SYSDEFS) -g

OBJS = pgsql_storage.o GoodsSample.o ClientStorage.o DatabaseRoot.o Dimensions.o DimensionDataLayer.o  FillDatabase.o  GoodsSampleApp.o  ItemDefinition.o  ItemDefinitionDataLayer.o  ItemDefinitionList.o  magaya_client_storage.o  Package.o  PackageDataLayer.o  PackageList.o  RootDataLayer.o  Storage.o  WarehouseItem.o  WarehouseItemDataLayer.o  WarehouseItemList.o  WarehouseReceipt.o  WarehouseReceiptDataLayer.o  WarehouseReceiptList.o 

INC = stdafx.h inc/ClientStorage.h inc/DatabaseRoot.h inc/DBCmdEx.h inc/DimensionDataLayer.h inc/Dimensions.h inc/FillDatabase.h inc/goods_container.h inc/GoodsSampleApp.h inc/GoodsTLCache.h inc/IndexHelper.h inc/ItemDefinitionDataLayer.h inc/ItemDefinition.h inc/ItemDefinitionList.h inc/magaya_client_storage.h inc/PackageDataLayer.h inc/Package.h inc/PackageList.h inc/RootDataLayer.h inc/Storage.h inc/WarehouseItemDataLayer.h inc/WarehouseItem.h inc/WarehouseItemList.h inc/WarehouseReceiptDataLayer.h inc/WarehouseReceipt.h inc/WarehouseReceiptList.h goods-diff/pgsql_storage.h


all: GoodsSample

GoodsSample: $(OBJS)
	$(LD) $(LDFLAGS) -o GoodsSample $(OBJS) ../goods/lib/libclient.a -lpqxx -lz

pgsql_storage.o: goods-diff/pgsql_storage.cxx goods-diff/pgsql_storage.h
	$(CC) $(CCFLAGS) goods-diff/pgsql_storage.cxx

GoodsSample.o: GoodsSample.cpp $(INC)
	$(CC) $(CCFLAGS) GoodsSample.cpp 

DatabaseRoot.o: src/DatabaseRoot.cpp $(INC)
	$(CC) $(CCFLAGS) src/DatabaseRoot.cpp

ClientStorage.o: src/ClientStorage.cpp $(INC)
	$(CC) $(CCFLAGS) src/ClientStorage.cpp

Dimensions.o: src/Dimensions.cpp $(INC)
	$(CC) $(CCFLAGS) src/Dimensions.cpp

DimensionDataLayer.o: src/DimensionDataLayer.cpp $(INC)
	$(CC) $(CCFLAGS) src/DimensionDataLayer.cpp

FillDatabase.o: src/FillDatabase.cpp $(INC)
	$(CC) $(CCFLAGS) src/FillDatabase.cpp

GoodsSampleApp.o: src/GoodsSampleApp.cpp $(INC)
	$(CC) $(CCFLAGS) src/GoodsSampleApp.cpp

ItemDefinition.o: src/ItemDefinition.cpp $(INC)
	$(CC) $(CCFLAGS) src/ItemDefinition.cpp

ItemDefinitionDataLayer.o: src/ItemDefinitionDataLayer.cpp $(INC)
	$(CC) $(CCFLAGS) src/ItemDefinitionDataLayer.cpp

ItemDefinitionList.o: src/ItemDefinitionList.cpp $(INC)
	$(CC) $(CCFLAGS) src/ItemDefinitionList.cpp

magaya_client_storage.o: src/magaya_client_storage.cpp $(INC)
	$(CC) $(CCFLAGS) src/magaya_client_storage.cpp

Package.o: src/Package.cpp $(INC)
	$(CC) $(CCFLAGS) src/Package.cpp

PackageDataLayer.o: src/PackageDataLayer.cpp $(INC)
	$(CC) $(CCFLAGS) src/PackageDataLayer.cpp

PackageList.o: src/PackageList.cpp $(INC)
	$(CC) $(CCFLAGS) src/PackageList.cpp

RootDataLayer.o: src/RootDataLayer.cpp $(INC)
	$(CC) $(CCFLAGS) src/RootDataLayer.cpp

Storage.o: src/Storage.cpp $(INC)
	$(CC) $(CCFLAGS) src/Storage.cpp

WarehouseItem.o: src/WarehouseItem.cpp $(INC)
	$(CC) $(CCFLAGS) src/WarehouseItem.cpp

WarehouseItemDataLayer.o: src/WarehouseItemDataLayer.cpp $(INC)
	$(CC) $(CCFLAGS) src/WarehouseItemDataLayer.cpp

WarehouseItemList.o: src/WarehouseItemList.cpp $(INC)
	$(CC) $(CCFLAGS) src/WarehouseItemList.cpp

WarehouseReceipt.o: src/WarehouseReceipt.cpp $(INC)
	$(CC) $(CCFLAGS) src/WarehouseReceipt.cpp

WarehouseReceiptDataLayer.o: src/WarehouseReceiptDataLayer.cpp $(INC)
	$(CC) $(CCFLAGS) src/WarehouseReceiptDataLayer.cpp

WarehouseReceiptList.o: src/WarehouseReceiptList.cpp $(INC)
	$(CC) $(CCFLAGS) src/WarehouseReceiptList.cpp

clean:
	rm -f *.o GoodsSample

