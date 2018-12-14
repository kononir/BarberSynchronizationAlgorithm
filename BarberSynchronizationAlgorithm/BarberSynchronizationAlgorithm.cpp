// BarberSynchronizationAlgorithm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#define NUMBER_OF_CHAIRS 3
#define NUMBER_OF_BARBERS 1
#define WORKING_TIME 5000
#define TIME_BETWEEN_CUSTOMERS 1000
#define MAX_NUMBER_OF_CUSTOMERS 1000

using namespace std;

HANDLE hCustomers;
HANDLE hQueueAccess;

typedef struct customersQueuePlace
{
	int customerIndex;
	bool freeFlag;
	HANDLE hPermition;
} customersQueuePlace;

deque<customersQueuePlace> customersQueue;

unsigned __stdcall barber(void* pArguments) {
	deque<customersQueuePlace>::iterator iter = customersQueue.begin();
	while (iter != customersQueue.end()) {
		WaitForSingleObject((*iter).hPermition, INFINITE);
		iter++;
	}

	while (true) {
		printf("Barber wants to invite next customer\n");
		WaitForSingleObject(hCustomers, INFINITE);
		WaitForSingleObject(hQueueAccess, INFINITE);

		printf("Barber invites customer\n");
		ReleaseMutex(customersQueue.front().hPermition);

		printf("Barber is working\n");
		Sleep(WORKING_TIME);

		WaitForSingleObject(customersQueue.front().hPermition, INFINITE);

		customersQueuePlace rearrangingPlace = customersQueue.front();
		customersQueue.pop_front();
		customersQueue.push_front(rearrangingPlace);

		ReleaseMutex(hQueueAccess);

		ReleaseSemaphore(hCustomers, 1, NULL);
	}

	_endthreadex(0);
	return 0;
}

unsigned __stdcall customer(void* pArguments) {
	int currentIndex = (int)pArguments;

	printf("Customer %d is coming and checking queue\n", currentIndex);

	if (ReleaseSemaphore(hCustomers, 1, NULL)) {
		WaitForSingleObject(hQueueAccess, INFINITE);

		printf("Customer %d is getting place in queue\n", currentIndex);

		deque<customersQueuePlace>::iterator iter = customersQueue.begin();
		while ((*iter).freeFlag == true) {
			iter++;
		}

		(*iter).freeFlag = false;
		(*iter).customerIndex = currentIndex;

		WaitForSingleObject((*iter).hPermition, INFINITE);

		printf("Customer %d is getting haircut\n", currentIndex);
		Sleep(WORKING_TIME);

		ReleaseMutex((*iter).hPermition);

		ReleaseMutex(hQueueAccess);
	}
	else {
		printf("All chairs are filled\n");
	}

	printf("Customer %d is leaving barber shop\n", currentIndex);

	_endthreadex(0);
	return 0;
}

int main()
{
	int numOfCustomers;

	do {
		cout << "Input number of customers: ";
		cin >> numOfCustomers;
	} while (numOfCustomers > MAX_NUMBER_OF_CUSTOMERS);

	hCustomers = CreateSemaphore(NULL, 0, NUMBER_OF_CHAIRS, NULL);
	hQueueAccess = CreateMutex(NULL, FALSE, NULL);

	customersQueue.resize(numOfCustomers);
	for (int placeIndex = 0; placeIndex < numOfCustomers; placeIndex++) {
		HANDLE hPermition = CreateMutex(NULL, FALSE, NULL);

		customersQueuePlace place = { 0, false, hPermition };
		customersQueue.push_back(place);
	}

	HANDLE hBarberThread = (HANDLE)_beginthreadex(NULL, 0, &barber, NULL, 0, NULL);

	vector<HANDLE> customerThreads(numOfCustomers);

	int currCustomerIndex = 0;

	// ограничить количество потоков!

	while (!_kbhit()) {
		Sleep(TIME_BETWEEN_CUSTOMERS);

		for (int customerThreadIndex = 0; customerThreadIndex < numOfCustomers; customerThreadIndex++) {
			DWORD isStillActive;
			
			GetExitCodeThread(customerThreads[customerThreadIndex], &isStillActive);

			if (isStillActive != STILL_ACTIVE) {
				CloseHandle(customerThreads[customerThreadIndex]);

				customerThreads[customerThreadIndex] = (HANDLE)_beginthreadex(NULL, 0, &customer, (void*)currCustomerIndex, 0, NULL);

				currCustomerIndex++;

				break;
			}
		}
	}

	for (vector<HANDLE>::iterator iter = customerThreads.begin(); iter != customerThreads.end(); iter++) {
		WaitForSingleObject((*iter), INFINITE);

		CloseHandle(*iter);
	}

	CloseHandle(hBarberThread);

	CloseHandle(hCustomers);
	CloseHandle(hQueueAccess);

	system("pause");

	return 0;
}
