// BarberSynchronizationAlgorithm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#define NUMBER_OF_CHAIRS 3
#define NUMBER_OF_BARBERS 1
#define WORKING_TIME 5000
#define TIME_BETWEEN_CUSTOMERS 1000

using namespace std;

int numOfWaitingCustomers = 0;

HANDLE hCustomers;
HANDLE hBarber;
HANDLE hWaiting;

unsigned __stdcall barber(void* pArguments) {
	while (true) {
		cout << "Barber is waiting customer (sleep)\n";
		WaitForSingleObject(hCustomers, INFINITE);

		WaitForSingleObject(hWaiting, INFINITE);
		cout << "Barber is getting access to number of waiting customers\n";

		numOfWaitingCustomers--;

		ReleaseSemaphore(hBarber, 1, NULL);
		cout << "Barber is ready to work\n";

		ReleaseMutex(hWaiting);

		cout << "Barber is working\n";
		Sleep(WORKING_TIME);
	}

	_endthreadex(0);
	return 0;
}

unsigned __stdcall customer(void* pArguments) {
	int currentIndex = (int)pArguments;

	cout << "Customer " << currentIndex << " is coming\n";

	WaitForSingleObject(hWaiting, INFINITE);

	if (numOfWaitingCustomers < NUMBER_OF_CHAIRS) {
		numOfWaitingCustomers++;

		ReleaseSemaphore(hCustomers, 1, NULL);

		cout << "Customer " << currentIndex << " is getting place in queue\n";

		ReleaseMutex(hWaiting);

		WaitForSingleObject(hBarber, INFINITE);

		cout << "Customer " << currentIndex << " is getting haircut\n";

		Sleep(WORKING_TIME);
	}
	else {
		ReleaseMutex(hWaiting);
	}

	cout << "Customer " << currentIndex << " is leaving barber shop\n";

	_endthreadex(0);
	return 0;
}

int main(int argc, char *argv[])
{
	//int numOfCustomers = atoi(argv[0]);

	int numOfCustomers;

	cout << "Input number of customers: ";
	cin >> numOfCustomers;
	
	hCustomers = CreateSemaphore(NULL, 0, numOfCustomers, NULL);
	hBarber = CreateSemaphore(NULL, 0, NUMBER_OF_BARBERS, NULL);
	hWaiting = CreateMutex(NULL, FALSE, NULL);

	HANDLE hBarberThread = (HANDLE)_beginthreadex(NULL, 0, &barber, NULL, 0, NULL);

	vector<HANDLE> customerHandles(numOfCustomers);

	int currCustomerIndex = 0;

	while (!_kbhit()) {
		Sleep(TIME_BETWEEN_CUSTOMERS);

		customerHandles.push_back((HANDLE)_beginthreadex(NULL, 0, &customer, (void*)currCustomerIndex, 0, NULL));

		currCustomerIndex++;
	}

	for (vector<HANDLE>::iterator iter = customerHandles.begin(); iter != customerHandles.end(); iter++) {
		CloseHandle(*iter);
	}

	CloseHandle(hBarberThread);

	CloseHandle(hCustomers);
	CloseHandle(hBarber);
	CloseHandle(hWaiting);

	system("pause");

	return 0;
}
