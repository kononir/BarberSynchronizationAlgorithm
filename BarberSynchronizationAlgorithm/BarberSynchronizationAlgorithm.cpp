// BarberSynchronizationAlgorithm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#define NUMBER_OF_CHAIRS 3
#define WORKING_TIME 5000
#define MAX_TIME_BETWEEN_CUSTOMERS 1000
#define MAX_NUMBER_OF_CUSTOMERS 20

using namespace std;

HANDLE hCustomers;
HANDLE hPermitionsAccess;
HANDLE* hPermitions;
HANDLE hLogger;
HANDLE hCurrCustomerN;

int currCustomerN = 0;
bool* freeFlags;
//int currentPermitionIndex = 0;
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
		
		logingWithoutParam("Barber invites customer\n");	



		ReleaseMutex(hPermitions[0]);
		WaitForSingleObject(hPermitions[0], INFINITE);



		logingWithoutParam("Barber is working\n");
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



			//WaitForSingleObject(hPermitionsAccess, INFINITE);

			//int rezultOfWaiting = (int)WaitForMultipleObjects(NUMBER_OF_CHAIRS, hPermitions, FALSE, INFINITE);	// ошибка с захватом мьютекса, который должен быть уже захвачен парикмахером
			//int permitionIndex = rezultOfWaiting - (int)WAIT_OBJECT_0;

			int permitionIndex = 0;
			while (true) {
				WaitForSingleObject(hPermitionsAccess, INFINITE);

				bool placeIsFree = freeFlags[permitionIndex] == true;

				ReleaseMutex(hPermitionsAccess);



				if (placeIsFree) {
					WaitForSingleObject(hPermitionsAccess, INFINITE);

					freeFlags[permitionIndex] = false;

					ReleaseMutex(hPermitionsAccess);



					WaitForSingleObject(hPermitions[permitionIndex], INFINITE);
					


					WaitForSingleObject(hPermitionsAccess, INFINITE);
					
					if (ReleaseMutex(hPermitions[0]) == 0) {
						cout << "ERROR!!!" << endl;
					}

					freeFlags[permitionIndex] = true;



					// сдвиг текущего индекса разрешения
					HANDLE firstPermition = hPermitions[0];
					bool firstFlag = freeFlags[0];
					for (int permitionIndex = 1; permitionIndex < NUMBER_OF_CHAIRS; permitionIndex++) {
						hPermitions[permitionIndex - 1] = hPermitions[permitionIndex];
						freeFlags[permitionIndex - 1] = freeFlags[permitionIndex];
					};
					hPermitions[NUMBER_OF_CHAIRS - 1] = firstPermition;
					freeFlags[NUMBER_OF_CHAIRS - 1] = firstFlag;

					//currentPermitionIndex++;

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
	freeFlags = new bool[NUMBER_OF_CHAIRS];

	for (int placeIndex = 0; placeIndex < NUMBER_OF_CHAIRS; placeIndex++) {
		hPermitions[placeIndex] = CreateMutex(NULL, FALSE, NULL);
		freeFlags[placeIndex] = true;
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
