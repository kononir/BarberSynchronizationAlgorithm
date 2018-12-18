// BarberSynchronizationAlgorithm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#define NUMBER_OF_CHAIRS 3
#define WORKING_TIME 5000
#define WAITING_TIME 100
#define MAX_TIME_BETWEEN_CUSTOMERS 3000
#define MAX_NUMBER_OF_CUSTOMERS 50
#define MIN_NUMBER_OF_CUSTOMERS 0

typedef struct place {
	int customerIndex;
	bool freeFlag;
} place;

HANDLE hCustomers;
HANDLE hPermitionsAccess;
HANDLE* hPermitions;
HANDLE hLogger;
HANDLE hCurrNumberOfPassedCustomers;

int currNumberOfPassedCustomers = 0;
int currNumberOfFreeChairs = NUMBER_OF_CHAIRS;

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

void logingWithoutParams(const char* text) {
	WaitForSingleObject(hLogger, INFINITE);
	printTimeStamp();
	printf(text);
	fprintf(stream, text);
	ReleaseMutex(hLogger);
}

void logingWithOneParam(const char* text, int param) {
	WaitForSingleObject(hLogger, INFINITE);
	printTimeStamp();
	printf(text, param);
	fprintf(stream, text, param);
	ReleaseMutex(hLogger);
}

void logingWithTwoParams(const char* text, int param1, int param2) {
	WaitForSingleObject(hLogger, INFINITE);
	printTimeStamp();
	printf(text, param1, param2);
	fprintf(stream, text, param1, param2);
	ReleaseMutex(hLogger);
}

unsigned __stdcall barber(void* pArguments) {
	WaitForMultipleObjects(NUMBER_OF_CHAIRS, hPermitions, TRUE, INFINITE);

	int numOfCustomers = (int)pArguments;

	logingWithoutParams("Barber is coming\n");

	while (true) {
		// Данная задержка нужна для того, чтобы посетитель успел перехватить разрешение на стрижку 
		// (без него после выдачи разрешения парикмахером его будет тут же перехватывать сам парикмахер) 
		Sleep(WAITING_TIME); 

		//-----------------Проверяем все ли посетители прошли----------------------------------------------//
		WaitForSingleObject(hCurrNumberOfPassedCustomers, INFINITE);

		bool allCustomersPassed = currNumberOfPassedCustomers == numOfCustomers;

		ReleaseMutex(hCurrNumberOfPassedCustomers);

		//-----------------Если все посетители прошли - выходим из цикла-----------------------------------//
		if (allCustomersPassed) {
			break;
		}

		//-----------------Если в очереди нет клиентов, то возвращаемся к началу цикла---------------------//
		if (!ReleaseSemaphore(hCustomers, 1, NULL)) {
			continue;
		}

		WaitForSingleObject(hPermitionsAccess, INFINITE);

		int currentIndex = places[0].customerIndex;

		ReleaseMutex(hPermitionsAccess);

		logingWithOneParam("Barber invites customer %d\n", currentIndex);
		ReleaseMutex(hPermitions[0]);
		WaitForSingleObject(hPermitions[0], INFINITE);

		WaitForSingleObject(hPermitionsAccess, INFINITE);

		//-----------------Сдвигаем очередь, освобождаем место-----------------------------------//
		HANDLE firstPermition = hPermitions[0];
		place firstPlace = places[0];
		for (int permitionIndex = 1; permitionIndex < NUMBER_OF_CHAIRS; permitionIndex++) {
			hPermitions[permitionIndex - 1] = hPermitions[permitionIndex];
			places[permitionIndex - 1] = places[permitionIndex];
		};
		hPermitions[NUMBER_OF_CHAIRS - 1] = firstPermition;
		places[NUMBER_OF_CHAIRS - 1] = firstPlace;

		places[NUMBER_OF_CHAIRS - 1].freeFlag = true;

		currNumberOfFreeChairs++;

		ReleaseMutex(hPermitionsAccess);

		logingWithOneParam("Barber is cutting off customer %d\n", currentIndex);
		Sleep(WORKING_TIME);

		logingWithoutParams("Barber is free\n");
	}

	logingWithoutParams("Barber is leaving barber shop\n");

	_endthread();
	return 0;
}

unsigned __stdcall customer(void* pArguments) {
	int currentIndex = (int)pArguments;

	logingWithOneParam("Customer %d is coming and checking queue\n", currentIndex);

	DWORD decision = WaitForSingleObject(hCustomers, 100);

	switch (decision) {
		case WAIT_OBJECT_0:
		{
			WaitForSingleObject(hPermitionsAccess, INFINITE);

			bool enoughtChairs = currNumberOfFreeChairs > 0;
			currNumberOfFreeChairs--;

			ReleaseMutex(hPermitionsAccess);

			if (enoughtChairs) {
				WaitForSingleObject(hPermitionsAccess, INFINITE);

				logingWithOneParam("Customer %d is searching free place in queue\n", currentIndex);
				int permitionIndex = 0;
				while (true) {
					bool placeIsFree = places[permitionIndex].freeFlag == true;

					if (placeIsFree) {
						break;
					}

					permitionIndex++;
				}

				logingWithTwoParams("Customer %d is getting place %d in queue\n", currentIndex, permitionIndex);
				places[permitionIndex].freeFlag = false;
				places[permitionIndex].customerIndex = currentIndex;

				ReleaseMutex(hPermitionsAccess);

				logingWithOneParam("Customer %d is waiting permition to haircut\n", currentIndex);
				WaitForSingleObject(hPermitions[permitionIndex], INFINITE);
				ReleaseMutex(hPermitions[0]);

				logingWithOneParam("Customer %d is getting haircut\n", currentIndex);
				Sleep(WORKING_TIME);

				break;
			}
			else {
				logingWithOneParam("Customer %d find out that chairs are filled\n", currentIndex);

				break;
			}
		}

		case WAIT_TIMEOUT:
		{
			logingWithOneParam("Customer %d find out that chairs are filled\n", currentIndex);

			break;
		}
	}

	logingWithOneParam("Customer %d is leaving barber shop\n", currentIndex);

	WaitForSingleObject(hCurrNumberOfPassedCustomers, INFINITE);
	currNumberOfPassedCustomers++;
	ReleaseMutex(hCurrNumberOfPassedCustomers);

	_endthread();
	return 0;
}

int main()
{
	int numOfCustomers;

	do {
		cout << "Input number of customers (max - " << MAX_NUMBER_OF_CUSTOMERS <<"): ";
		cin >> numOfCustomers;
	} while (numOfCustomers > MAX_NUMBER_OF_CUSTOMERS && numOfCustomers < MIN_NUMBER_OF_CUSTOMERS);

	srand((int)time(NULL));

	fopen_s(&stream, "log.txt", "w");
	hLogger = CreateMutex(NULL, FALSE, NULL);

	hCustomers = CreateSemaphore(NULL, NUMBER_OF_CHAIRS, NUMBER_OF_CHAIRS, NULL);
	hPermitionsAccess = CreateMutex(NULL, FALSE, NULL);
	hCurrNumberOfPassedCustomers = CreateMutex(NULL, FALSE, NULL);

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

	for (int placeIndex = 0; placeIndex < NUMBER_OF_CHAIRS; placeIndex++) {
		CloseHandle(hPermitions[placeIndex]);
	}

	CloseHandle(hCustomers);
	CloseHandle(hPermitionsAccess);
	CloseHandle(hCurrNumberOfPassedCustomers);

	fclose(stream);

	system("pause");

	return 0;
}
