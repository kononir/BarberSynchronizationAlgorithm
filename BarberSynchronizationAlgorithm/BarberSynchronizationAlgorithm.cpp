// BarberSynchronizationAlgorithm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#define NUMBER_OF_CHAIRS 3
#define WORKING_TIME 5000
#define MAX_TIME_BETWEEN_CUSTOMERS 1000
#define MAX_NUMBER_OF_CUSTOMERS 20

typedef struct place {
	int customerIndex;
	bool freeFlag;
} place;

HANDLE hCustomers;
HANDLE hPermitionsAccess;
HANDLE* hPermitions;
HANDLE hLogger;
HANDLE hCurrCustomerN;

int currCustomerN = 0;

place* places;
FILE* stream;

using namespace std;

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

void logingWithoutParam(const char* text) {
	WaitForSingleObject(hLogger, INFINITE);
	printTimeStamp();
	printf(text);
	fprintf(stream, text);
	ReleaseMutex(hLogger);
}

void logingWithParam(const char* text, int customerIndex) {
	WaitForSingleObject(hLogger, INFINITE);
	printTimeStamp();
	printf(text, customerIndex);
	fprintf(stream, text, customerIndex);
	ReleaseMutex(hLogger);
}

unsigned __stdcall barber(void* pArguments) {
	WaitForMultipleObjects(NUMBER_OF_CHAIRS, hPermitions, TRUE, INFINITE);

	int numOfCustomers = (int)pArguments;

	logingWithoutParam("Barber is coming\n");

	while (true) {
		WaitForSingleObject(hCurrCustomerN, INFINITE);

		bool allCustomersPassed = currCustomerN == numOfCustomers;

		ReleaseMutex(hCurrCustomerN);

		if (allCustomersPassed) {
			break;
		}



		if (!ReleaseSemaphore(hCustomers, 1, NULL)) {
			continue;
		}

		int currentIndex = places[0].customerIndex;
		
		logingWithParam("Barber invites customer %d\n", currentIndex);



		ReleaseMutex(hPermitions[0]);
		WaitForSingleObject(hPermitions[0], INFINITE);



		logingWithParam("Barber is cutting off customer %d\n", currentIndex);
		Sleep(WORKING_TIME);



		logingWithoutParam("Barber is free\n");
	}

	_endthread();
	return 0;
}

unsigned __stdcall customer(void* pArguments) {
	int currentIndex = (int)pArguments;

	logingWithParam("Customer %d is coming and checking queue\n", currentIndex);

	DWORD decision = WaitForSingleObject(hCustomers, 100);

	switch (decision) {
		case WAIT_OBJECT_0:
		{
			logingWithParam("Customer %d is getting place in queue\n", currentIndex);



			int permitionIndex = 0;
			while (true) {
				WaitForSingleObject(hPermitionsAccess, INFINITE);

				bool placeIsFree = places[permitionIndex].freeFlag == true;

				ReleaseMutex(hPermitionsAccess);



				if (placeIsFree) {
					WaitForSingleObject(hPermitionsAccess, INFINITE);

					places[permitionIndex].freeFlag = false;
					places[permitionIndex].customerIndex = currentIndex;

					ReleaseMutex(hPermitionsAccess);



					WaitForSingleObject(hPermitions[permitionIndex], INFINITE);
					


					WaitForSingleObject(hPermitionsAccess, INFINITE);
					
					if (ReleaseMutex(hPermitions[0]) == 0) {
						cout << "ERROR!!!" << endl;
					}

					places[permitionIndex].freeFlag = true;



					// сдвиг текущего индекса разрешения
					HANDLE firstPermition = hPermitions[0];
					place firstPlace = places[0];
					for (int permitionIndex = 1; permitionIndex < NUMBER_OF_CHAIRS; permitionIndex++) {
						hPermitions[permitionIndex - 1] = hPermitions[permitionIndex];
						places[permitionIndex - 1] = places[permitionIndex];
					};
					hPermitions[NUMBER_OF_CHAIRS - 1] = firstPermition;
					places[NUMBER_OF_CHAIRS - 1] = firstPlace;



					ReleaseMutex(hPermitionsAccess);

					break;
				}

				permitionIndex++;
			}



			logingWithParam("Customer %d is getting haircut\n", currentIndex);
			Sleep(WORKING_TIME);

			break;
		}

		case WAIT_TIMEOUT:
		{
			logingWithParam("Customer %d find out that chairs are filled\n", currentIndex);

			break;
		}
	}

	logingWithParam("Customer %d is leaving barber shop\n", currentIndex);

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
	places = new place[NUMBER_OF_CHAIRS];

	for (int placeIndex = 0; placeIndex < NUMBER_OF_CHAIRS; placeIndex++) {
		hPermitions[placeIndex] = CreateMutex(NULL, FALSE, NULL);
		places[placeIndex] = { 0, true };
	}

	HANDLE hBarber = (HANDLE)_beginthread((_beginthread_proc_type)barber, 0, (void*)numOfCustomers);

	int currCustomerIndex = 0;

	while (currCustomerIndex < numOfCustomers) {
		Sleep(rand() % MAX_TIME_BETWEEN_CUSTOMERS);
		
		_beginthread((_beginthread_proc_type)customer, 0, (void*)currCustomerIndex);

		currCustomerIndex++;
	}
	
	WaitForSingleObject(hBarber, INFINITE);

	CloseHandle(hCustomers);
	CloseHandle(hPermitionsAccess);

	system("pause");

	return 0;
}
