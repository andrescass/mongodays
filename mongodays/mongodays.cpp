// mongodays.cpp : Este archivo contiene la función "main". La ejecución del programa comienza y termina ahí.
//

#include "pch.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <time.h>
#include <thread>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>

#include "csv.h"

using namespace csv;

static const std::string APP_VERSION = "1.3.0";

class dayClass
{
public:
	std::string day;
	std::vector<std::string> hour;
	std::vector<float> open;
	std::vector<float> high;
	std::vector<float> low;
	std::vector<float> close;

	dayClass(std::string d)
		:day(d)
	{}

	void dayPushback(std::string a, float o, float h, float l, float c)
	{
		hour.push_back(a);
		open.push_back(o);
		high.push_back(h);
		low.push_back(l);
		close.push_back(c);
		return;
	};

};

void showHelpMsg()
{
	std::cout << "version " << APP_VERSION << std::endl << std::endl;
	std::cout << "Usage: daycomp.exe -i file1.csv file2.csv -o output -t [min/sec] " << std::endl << std::endl;
	std::cout << "\t -i input csv files' names. Files' names must not contain white spaces" << std::endl;
	std::cout << "\t -h help. Show this message " << std::endl << std::endl;

	return;
}

std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> result;
	std::stringstream ss(s);
	std::string item;

	while (getline(ss, item, delim)) {
		result.push_back(item);
	}

	return result;
}

//void process_day(std::string dbname, mongocxx::client& client, dayClass day)
auto process_day = [](std::string dbname, mongocxx::client& client, dayClass day)
{
	std::vector<bsoncxx::document::value> doch;
	std::vector<bsoncxx::document::value> docl;
	float lowF;
	float highF;
	long int counter = 0;
	std::string pS;
	std::vector<std::string> lowDays;
	std::vector<std::string> highDays;

	for (int i = 0; i < day.low.size()-1; i++)
	{
		for (long int j = (i + 1); j < day.low.size(); j++) //day.low.size()
		{
			lowF = day.low[i] - day.low[j];
			highF = day.high[i] - day.high[j];
			if (lowF < 4)
			{
				pS = day.hour[i];
				pS.append("-").append(day.hour[j]);
				//lowDays.push_back(pS);
				docl.push_back(
					bsoncxx::builder::stream::document{} << "slot" << pS << bsoncxx::builder::stream::finalize);
			}
			if (highF > -4)
			{
				pS = day.hour[i];
				pS.append("-").append(day.hour[j]);
				doch.push_back(
					bsoncxx::builder::stream::document{} << "slot" << pS << bsoncxx::builder::stream::finalize);
			}
			/*
			mu.lock();
			std::cout << begin << " " << day.hour[i] << "-" << day.hour[j] << std::endl;
			mu.unlock();
			*/
		}
		auto coll = client[dbname]["low"].insert_many(docl);
		auto colh = client[dbname]["high"].insert_many(doch);
	}


};

int main(int argc, char **argv)
{
	time_t t = time(NULL);
	struct tm *startT = gmtime(&t);
	int stH = startT->tm_hour;
	int stM = startT->tm_min;
	int stS = startT->tm_sec;
	struct tm *endT;
	std::vector<std::string> in_v;
	std::vector<std::string> inputFileNames;
	std::stringstream ss;

	// Argument parsing
	if (argc > 1)
	{
		for (int argi = 1; argi < argc; argi++)
		{
			if (strcmp(argv[argi], "-i") == 0)
			{
				// input files
				argi++;
				if ((argi < argc) && (argv[argi][0] != '-'))
				{
					while ((argi < argc) && (argv[argi][0] != '-'))
					{
						inputFileNames.push_back(argv[argi]);
						argi++;
					}

					if (argi < argc)
					{
						argi--;
					}
				}
				else
				{
					std::cout << "Error in input file name. Use -h for help" << std::endl;
					return 0;
				}
			}
			else if (strcmp(argv[argi], "-h") == 0)
			{
				showHelpMsg();
				return 0;
			}
		}
	}
	else
	{
		//inputFileName = "./the super boring stuf1.csv";
		showHelpMsg();
		return 0;
	}

	std::cout << "Start processing at " << startT->tm_hour << ":" << startT->tm_min << ":" << startT->tm_sec << std::endl;

	std::vector<dayClass> oneDay;

	CSVFormat format;
	format.delimiter(',');
	format.variable_columns(false); // Short-hand
	int dayIdx = 0;
	//format.trim({ ' ', '\t' });
	//format.variable_columns(VariableColumnPolicy::IGNORE);
	for (auto& filen : inputFileNames)
	{
		try
		{
			CSVReader reader(filen, format);


			CSVRow row;
			std::string currentDay = "";
			std::vector< std::string> parsStamp;

			for (CSVRow& row : reader)
			{
				if (row[" Open Price"].is_num())
				{
					parsStamp = split(row[" Time"].get<std::string>(), 'T');
					if (parsStamp[0].compare(currentDay) != 0) // nuevo día
					{
						currentDay = parsStamp[0];
						oneDay.push_back(dayClass(parsStamp[0]));
						dayIdx++;
					}
					oneDay[dayIdx - 1].dayPushback(parsStamp[1], atof(row[" Open Price"].get().c_str())
						, atof(row[" High Price"].get().c_str())
						, atof(row[" Low Price"].get().c_str())
						, atof(row[" Close Price"].get().c_str()));
				}
			}
			std::cout << filen << " file readed \n";
		}
		catch (const char* e)
		{
			std::cout << "File error " << filen << e;
			exit(1);
		}
	}

	for (dayClass& d : oneDay)
	{
		std::cout << d.day << " with " << d.hour.size() << " entries " << std::endl;
	}

	// Insert in mongodb
	mongocxx::instance instance{};
	mongocxx::uri uri{ "mongodb://localhost:27017/?minPoolSize=2&maxPoolSize=4" };

	mongocxx::pool pool{ uri };

	std::string dbname = "days";
	std::string coll = "low";
	std::string colh = "high";

	std::vector<std::thread> t_threads;

	for (dayClass& d : oneDay)
	{
		//std::thread t1([&]() {
			//auto c = pool.acquire();
			//process_day(dbname, *c, d);
		//});
		t_threads.push_back(std::thread([&]() {
			auto c = pool.acquire();
			process_day(dbname, *c, d);
		}));
	}

	for (std::thread& t : t_threads)
	{
		t.join();
	}

	time_t te = time(NULL);
	endT = gmtime(&te);
	std::cout << "Start processing at " << stH << ":" << stM << ":" << stS << std::endl;
	std::cout << "End processing at " << endT->tm_hour << ":" << endT->tm_min << ":" << endT->tm_sec << std::endl;
}

// Ejecutar programa: Ctrl + F5 o menú Depurar > Iniciar sin depurar
// Depurar programa: F5 o menú Depurar > Iniciar depuración

// Sugerencias para primeros pasos: 1. Use la ventana del Explorador de soluciones para agregar y administrar archivos
//   2. Use la ventana de Team Explorer para conectar con el control de código fuente
//   3. Use la ventana de salida para ver la salida de compilación y otros mensajes
//   4. Use la ventana Lista de errores para ver los errores
//   5. Vaya a Proyecto > Agregar nuevo elemento para crear nuevos archivos de código, o a Proyecto > Agregar elemento existente para agregar archivos de código existentes al proyecto
//   6. En el futuro, para volver a abrir este proyecto, vaya a Archivo > Abrir > Proyecto y seleccione el archivo .sln
