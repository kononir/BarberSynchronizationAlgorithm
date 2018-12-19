// BarberSynchronizationAlgorithm.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#define NUMBER_OF_CHAIRS 3
#define WORKING_TIME 5000
#define WAITING_TIME 100
#define MAX_TIME_BETWEEN_CUSTOMERS 3000
#define MAX_NUMBER_OF_CUSTOMERS 50
#define MIN_NUMBER_OF_CUSTOMERS 0

typedef struct place {	// структура, описывающее место в очереди
	int customerIndex;	// индекс посетителя, занявшего место
	bool freeFlag;		// флаг определяющий, свобобно ли место
} place;

HANDLE hCustomers;						// семафор посетителей (говорит парикмахеру, есть ли посетители в очереди)
HANDLE hPermitionsAccess;				// мьютекс для работы с очередью и свободными стульями
HANDLE* hPermitions;					// массив мьютексов разрешений на стрижку
HANDLE hLogger;							// мьютекс логирования
HANDLE hCurrNumberOfPassedCustomers;	// мьютекс для работы с текущим количеством всех ушедших клиентов

int currNumberOfPassedCustomers = 0;			// текущее количество всех прошедших клиентов (переменная нужна, чтобы поток парикмахера завершался вовремя)
int currNumberOfFreeChairs = NUMBER_OF_CHAIRS;	// текущее количество свободных стульев (переменная нужна, чтобы не пропускать посетителей, которые пытаются зайти когда парикмахер освободил семафор, но свободных мест нет)

place* places;	// массив мест в очереди
FILE* stream;	// указатель на файл логирования

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
	// Занимаем все мьютексы разрешения на стрижку
	WaitForMultipleObjects(NUMBER_OF_CHAIRS, hPermitions, TRUE, INFINITE);

	// Параметр - количество всех клиентов, которые пройдут (вводилось пользователем)
	int numOfCustomers = (int)pArguments;

	logingWithoutParams("Barber is coming\n");

	while (true) {
		/* 
		Данная задержка нужна для того, чтобы посетитель успел перехватить разрешение на стрижку 
		(без неё, после выдачи разрешения парикмахером, разрешение будет тут же перехватывать сам парикмахер)
		*/
		Sleep(WAITING_TIME); 

		// Проверяем все ли посетители прошли
		WaitForSingleObject(hCurrNumberOfPassedCustomers, INFINITE);	// Занимаем мьютекс для работы с текущим количеством всех ушедших клиентов

		bool allCustomersPassed = currNumberOfPassedCustomers == numOfCustomers;

		ReleaseMutex(hCurrNumberOfPassedCustomers);	// Освобождаем мьютекс для работы с текущим количеством всех ушедших клиентов

		// Если все посетители прошли - выходим из цикла
		if (allCustomersPassed) {
			break;
		}

		// Если в очереди нет клиентов, то возвращаемся к началу цикла
		if (!ReleaseSemaphore(hCustomers, 1, NULL)) {
			continue;
		}

		int currentIndex = places[0].customerIndex;	// Берём id первого посетителя

		logingWithOneParam("Barber invites customer %d\n", currentIndex);
		ReleaseMutex(hPermitions[0]);	// Освобождаем мьютекс разрешения на стрижку (приглашаем посетителя)
		WaitForSingleObject(hPermitions[0], INFINITE);	// Занимаем освобождённый посетителем мьютекс разрешения

		WaitForSingleObject(hPermitionsAccess, INFINITE);	// Занимаем мьютекс для работы с очередью и количеством занятых стульев

		// Делаем циклический сдвиг очереди (массив мест и массив разрешений)
		HANDLE firstPermition = hPermitions[0];
		place firstPlace = places[0];
		for (int permitionIndex = 1; permitionIndex < NUMBER_OF_CHAIRS; permitionIndex++) {
			hPermitions[permitionIndex - 1] = hPermitions[permitionIndex];
			places[permitionIndex - 1] = places[permitionIndex];
		};
		hPermitions[NUMBER_OF_CHAIRS - 1] = firstPermition;
		places[NUMBER_OF_CHAIRS - 1] = firstPlace;

		places[NUMBER_OF_CHAIRS - 1].freeFlag = true;	// Устанавливаем флаг последнего места в свободное состояние

		currNumberOfFreeChairs++;	// Увеличиваем количество свободных мест

		ReleaseMutex(hPermitionsAccess);	// Освобождаем мьютекс для работы с очередью и количеством занятых стульев

		logingWithOneParam("Barber is cutting off customer %d\n", currentIndex);
		Sleep(WORKING_TIME);	// Парикмахер стрижёт посетителя

		logingWithoutParams("Barber is free\n");
	}

	logingWithoutParams("Barber is leaving barber shop\n");

	_endthread();
	return 0;
}

unsigned __stdcall customer(void* pArguments) {
	int currentIndex = (int)pArguments;	// Параметр - текущий индекс посетителя

	logingWithOneParam("Customer %d is coming and checking queue\n", currentIndex);

	DWORD decision = WaitForSingleObject(hCustomers, 100);	// Пытаемся увеличить семафор посетителей; после 100 мс возвращет код ошибки таймаута

	switch (decision) {
		// Если удалось увеличить семафор, то работаем дальше
		case WAIT_OBJECT_0:
		{
			WaitForSingleObject(hPermitionsAccess, INFINITE);	// Занимаем мьютекс для работы с количеством занятых стульев

			bool enoughtChairs = currNumberOfFreeChairs > 0;	// Проверяем, есть ли свободные стулья

			ReleaseMutex(hPermitionsAccess);	// освобождаем мьютекс для работы с количеством занятых стульев

			// Если есть свободные стулья, то продолжаем работу
			if (enoughtChairs) {
				WaitForSingleObject(hPermitionsAccess, INFINITE);	// Занимаем мьютекс для работы с очередью и количеством занятых стульев

				logingWithOneParam("Customer %d is searching free place in queue\n", currentIndex);
				
				// Ищем в цикле свободное место в очереди
				int permitionIndex = 0;
				while (true) {
					bool placeIsFree = places[permitionIndex].freeFlag == true;

					if (placeIsFree) {
						break;
					}

					permitionIndex++;
				}

				logingWithTwoParams("Customer %d is getting place %d in queue\n", currentIndex, permitionIndex);
				places[permitionIndex].freeFlag = false;	// Освобождаем место в очереди
				places[permitionIndex].customerIndex = currentIndex;	// Присваиваем индексу посетителя, занявшему место, индекс текущего посетителя

				currNumberOfFreeChairs--;	// Уменьшаем на 1 количество свободных стульев

				ReleaseMutex(hPermitionsAccess);	// Освобождаем мьютекс для работы с очередью и количеством занятых стульев

				logingWithOneParam("Customer %d is waiting permition to haircut\n", currentIndex);
				WaitForSingleObject(hPermitions[permitionIndex], INFINITE);		// Ждём захвата мьютекса разрешения на стрижку (посетитель ждёт разрешения на стрижку от парикмахера)
				ReleaseMutex(hPermitions[0]);	// Освобождаем полученное разрешение на стрижку

				logingWithOneParam("Customer %d is getting haircut\n", currentIndex);
				Sleep(WORKING_TIME);	// Посетитель стрижётся

				break;
			}
			// Если стульев недостаточно, то говорим про это и уходим
			else {
				logingWithOneParam("Customer %d find out that chairs are filled\n", currentIndex);

				break;
			}
		}

		// Если произошёл таймаут, т.е. не успели увеличить семафор, то говорим, что все стулья заняты, и уходим
		case WAIT_TIMEOUT:
		{
			logingWithOneParam("Customer %d find out that chairs are filled\n", currentIndex);

			break;
		}
	}

	logingWithOneParam("Customer %d is leaving barber shop\n", currentIndex);

	WaitForSingleObject(hCurrNumberOfPassedCustomers, INFINITE);	// Занимаем мьютекс для работы с текущим количеством всех ушедших клиентов
	currNumberOfPassedCustomers++;	// Увеличиваем текущее количество ушедших посетителей
	ReleaseMutex(hCurrNumberOfPassedCustomers);	// Освобождаем мьютекс для работы с текущим количеством всех ушедших клиентов

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
	
	// Ждём, пока парикмахер не завершит работу
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
