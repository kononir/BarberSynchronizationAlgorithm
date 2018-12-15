// BarberSynchronizationAlgorithm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#define NUMBER_OF_CHAIRS 3
#define WORKING_TIME 5000
#define TIME_BETWEEN_CUSTOMERS 1000
#define MAX_NUMBER_OF_CUSTOMERS 1000

using namespace std;

HANDLE hCustomers;
HANDLE hPermitionsAccess;
HANDLE* hPermitions;
HANDLE hLogger;
HANDLE hCurrCustomerN;

int currCustomerN = 0;
FILE* stream;

void printTimeStamp()
{
	SYSTEMTIME timeStamp; // Текущее время. 
	GetLocalTime(&timeStamp); // Определить текущее время. 
	printf("%02d:%02d:%02d.%03d   ", // Вывести текущее время на экран. 
		timeStamp.wHour, timeStamp.wMinute,
		timeStamp.wSecond, timeStamp.wMilliseconds);
	fprintf(stream, "%02d:%02d:%02d.%03d   ", // Записать текущее время в файл. 
		timeStamp.wHour, timeStamp.wMinute,
		timeStamp.wSecond, timeStamp.wMilliseconds);
}

void logingBarber(const char* text) {
	WaitForSingleObject(hLogger, INFINITE);
	printTimeStamp();
	printf(text);
	fprintf(stream, text);
	ReleaseMutex(hLogger);
}

void logingCustomer(const char* text, int customerIndex) {
	WaitForSingleObject(hLogger, INFINITE);
	printTimeStamp();
	printf(text, customerIndex);
	fprintf(stream, text, customerIndex);
	ReleaseMutex(hLogger);
}

unsigned __stdcall barber(void* pArguments) {
	WaitForMultipleObjects(NUMBER_OF_CHAIRS, hPermitions, TRUE, INFINITE);

	int numOfCustomers = (int)pArguments;

	logingBarber("Barber is coming\n");

	while (true) {
		WaitForSingleObject(hCurrCustomerN, INFINITE);

		if (currCustomerN == numOfCustomers) {
			break;
		}

		ReleaseMutex(hCurrCustomerN);



		if (!ReleaseSemaphore(hCustomers, 1, NULL)) {
			continue;
		}
		
		logingBarber("Barber invites customer\n");	



		//WaitForSingleObject(hPermitionsAccess, INFINITE);

		ReleaseMutex(hPermitions[0]);
		WaitForSingleObject(hPermitions[0], INFINITE);

		//ReleaseMutex(hPermitionsAccess);



		logingBarber("Barber is working\n");
		Sleep(WORKING_TIME);

		

		//WaitForSingleObject(hPermitionsAccess, INFINITE);

		// сдвиг очереди
		HANDLE firstPermition = hPermitions[0];
		for (int permitionIndex = 1; permitionIndex > NUMBER_OF_CHAIRS; permitionIndex++) {
			hPermitions[permitionIndex - 1] = hPermitions[permitionIndex];
		};
		hPermitions[NUMBER_OF_CHAIRS - 1] = firstPermition;

		//ReleaseMutex(hPermitionsAccess);



		logingBarber("Barber is free\n");
	}

	_endthread();
	return 0;
}

unsigned __stdcall customer(void* pArguments) {
	int currentIndex = (int)pArguments;

	logingCustomer("Customer %d is coming and checking queue\n", currentIndex);

	DWORD decision = WaitForSingleObject(hCustomers, 1000);

	switch (decision) {
		case WAIT_OBJECT_0:
		{
			logingCustomer("Customer %d is getting place in queue\n", currentIndex);



			WaitForSingleObject(hPermitionsAccess, INFINITE);

			int rezultOfWaiting = (int)WaitForMultipleObjects(NUMBER_OF_CHAIRS, hPermitions, FALSE, INFINITE);	// ошибка с захватом мьютекса, который должен быть уже захвачен парикмахером
			int permitionIndex = rezultOfWaiting - (int)WAIT_OBJECT_0;
			ReleaseMutex(hPermitions[permitionIndex]);
			
			ReleaseMutex(hPermitionsAccess);



			logingCustomer("Customer %d is getting haircut\n", currentIndex);
			Sleep(WORKING_TIME);

			break;
		}

		case WAIT_TIMEOUT:
		{
			logingCustomer("All chairs are filled\n", currentIndex);

			break;
		}
	}

	logingCustomer("Customer %d is leaving barber shop\n", currentIndex);

	WaitForSingleObject(hCurrCustomerN, INFINITE);
	currCustomerN++;
	ReleaseMutex(hCurrCustomerN);

	_endthread();
	return 0;
}

int main()
{
	int numOfCustomers;

	do {
		cout << "Input number of customers: ";
		cin >> numOfCustomers;
	} while (numOfCustomers > MAX_NUMBER_OF_CUSTOMERS);

	srand((int)time(NULL));

	fopen_s(&stream, "log.txt", "w");
	hLogger = CreateMutex(NULL, FALSE, NULL);

	hCustomers = CreateSemaphore(NULL, NUMBER_OF_CHAIRS, NUMBER_OF_CHAIRS, NULL);
	hPermitionsAccess = CreateMutex(NULL, FALSE, NULL);
	hCurrCustomerN = CreateMutex(NULL, FALSE, NULL);

	hPermitions = new HANDLE[NUMBER_OF_CHAIRS];

	for (int placeIndex = 0; placeIndex < NUMBER_OF_CHAIRS; placeIndex++) {
		hPermitions[placeIndex] = CreateMutex(NULL, FALSE, NULL);
	}

	HANDLE hBarber = (HANDLE)_beginthread((_beginthread_proc_type)barber, 0, (void*)numOfCustomers);

	int currCustomerIndex = 0;

	while (currCustomerIndex < numOfCustomers) {
		Sleep(rand() % TIME_BETWEEN_CUSTOMERS);
		
		_beginthread((_beginthread_proc_type)customer, 0, (void*)currCustomerIndex);

		currCustomerIndex++;
	}
	
	WaitForSingleObject(hBarber, INFINITE);

	CloseHandle(hCustomers);
	CloseHandle(hPermitionsAccess);

	system("pause");

	return 0;
}
