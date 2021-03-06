#include <iostream>
#include <fstream>	// file I/O
#include <sstream>	// ostringstream
#include <cstdlib>	// rand
#include <ctime>	// time
#include <cmath>	// sqrt
#include <cstring>	// memcpy
#include <limits>   // numeric_limits

#include "VRPlib.h"

using namespace std;


/* Problem data variables */
int N;		// number of vertices [1..N]
int NVeh;	// number of vehicles [1..NVeh]
int Q;		// vehicle max capacity
double **c;
float *coordX; float *coordY;
int *demand;


/* Handle instance files of Cordeau-Laporte vrp/old */
void readInstanceFileCordeauLaporteVRPold(const char *filename) {
	int i;
	std::string line;
	std::ifstream instance_file;
	instance_file.open(filename);
	if (instance_file.fail()) {
		std::cerr << "Error: Unable to open " << filename << endl;
		exit (8);
	}

	instance_file >> i; 	// skip first integer
	instance_file >> NVeh;	// retrieve number of vehicles
	instance_file >> N;		// retrieve instance size
	c = new double*[NVeh+N+1];
	for(int i=0; i<NVeh+N+1; i++) c[i] = new double[NVeh+N+1];

	coordX = new float[NVeh+N+1]; 
	coordY = new float[NVeh+N+1];
	demand = new int[NVeh+N+1];
	std::getline(instance_file, line); // skip end of line

	instance_file >> i;			// skip int
	instance_file >> Q;			// retrieve max capacity

	// DEPOTS
	instance_file >> i;					// skip int
	instance_file >> coordX[1]; 		// retrieve x coordinate
	instance_file >> coordY[1]; 		// retrieve y coordinate
	std::getline(instance_file, line);	// skip end of line
	for (int r=2; r<NVeh+1; r++) {
		coordX[r] = coordX[1];
		coordY[r] = coordY[1];
	} 

	// REQUESTS
	for (int j=1; j<N+1; j++) {
		instance_file >> i; 				// skip int
		instance_file >> coordX[j+NVeh]; 	// retrieve x coordinate
		instance_file >> coordY[j+NVeh]; 	// retrieve y coordinate
		instance_file >> i;					// skip int
		instance_file >> demand[j+NVeh];	// retrieve demand
		std::getline(instance_file, line);	// skip end of line
	}

	instance_file.close();

	for (int i=1; i<NVeh+N+1; i++)
		for (int j=1; j<NVeh+N+1; j++)
			c[i][j] = std::sqrt(std::pow((coordX[i] - coordX[j]),2) + std::pow((coordY[i] - coordY[j]),2)); // compute Euclidean distances
}

/*
inline std::ostream &operator << (std::ostream &out_file, solutionTSP& s) {
	if (s.getViolations() > 0) out_file << "Infeasible ! ";
	out_file << "Cost=" << s.getCost() << " ";
	for(int i=1; i<N+1; i++)
		out_file << s.step[i] << " ";
	return (out_file);
}*/


/* class solutionVRP ********************************************************************************************
*	defines a solution for a TSP 
*/
solutionVRP::solutionVRP() {											// constructor ***
	previous = new int[NVeh+N+1];	// vertices from 1 to NVeh + N
	next = new int[NVeh+N+1];
	vehicle = new int[NVeh+N+1];	// vertices from 1 to Nveh + Nveh 	// vehicle[i] = i iff i is a depot
									//										vehicle[i] = 0 iff i is not inserted

	}
solutionVRP::solutionVRP(const solutionVRP& old_solution) {				// copy constructor ***
	previous = new int[NVeh+N+1];
	next = new int[NVeh+N+1];
	vehicle = new int[NVeh+N+1];
	*this = old_solution;
}
solutionVRP& solutionVRP::operator = (const solutionVRP& sol) {
	memcpy(previous, sol.previous, (NVeh+N+1)*sizeof(int));
	memcpy(next, sol.next, (NVeh+N+1)*sizeof(int));
	memcpy(vehicle, sol.vehicle, (NVeh+N+1)*sizeof(int));
	return (*this);
}
solutionVRP::~solutionVRP() {										// destructor ***
	delete [] previous;
	delete [] next;
	delete [] vehicle;
}

void solutionVRP::generateInitialSolution() {	// BEST INSERTION INITIAL SOL
												// todo: random vertex selection for insertion
	for(int r=1; r<NVeh+1; r++)	{
		vehicle[r] = r;
		previous[r] = r;
		next[r] = r;
	}

	/* To begin with, no customer vertex inserted */
	for (int i=NVeh+1; i<NVeh+N+1; i++) vehicle[i] = 0; 

	/* Then assign best insertion to each customer vertex in turn */
	for (int i=NVeh+1; i<NVeh+N+1; i++) {
		int before_i = bestInsertion(i);

		insertVertex(i, before_i, false);
	}
}

/* bestInsertion(int vertex)
	Find the best position to REinstert a single vertex "vertex"
*/
int solutionVRP::bestInsertion(int vertex) {
	int before_i;
	float min_delta = numeric_limits<float>::max();
	for (int i=1; i<NVeh+N+1; i++) {
		float delta = 0.0;
		if (i == vertex || i == next[vertex]) continue; // skip if same position
		if (vehicle[i] == 0) continue; // skip if vertex i not inserted yet
		delta =   c[previous[i]][i] 
				+ c[previous[i]][vertex]
				+ c[vertex][i];
		if (delta < min_delta) {
			before_i = i;
			min_delta = delta;
		} 
	} 
	return before_i;
}


void solutionVRP::insertVertex(int vertex, int before_i, bool remove) {
	//cout << "moving " << vertex-NVeh << " to before " << before_i-NVeh << endl;
	int from_route = vehicle[vertex];		// remember the route

	// Remove from current position
	if (remove) {
		previous[next[vertex]] = previous[vertex];
		next[previous[vertex]] = next[vertex];
	}

	// Insert elsewhere
	vehicle[vertex] = vehicle[before_i];
	next[vertex] = before_i;
	previous[vertex] = previous[before_i];
	previous[before_i] = vertex;
	next[previous[vertex]] = vertex;

	// notify solution that some routes changed
	if (remove) this->routeChange(from_route); 
	if (from_route != vehicle[before_i]) 
		this->routeChange(vehicle[before_i]);
}

inline float solutionVRP::getCost(int k) {	// returns cost of route k (all route cost of k==0)
	float cost = 0.0;
	for (int r=1; r<NVeh+1; r++) {
		if (k != 0 && k != r) continue;
		int i = previous[r];
		while (i != r) {
			cost += c[i][next[i]];
			i=previous[i];
		} cost += c[i][next[i]];
	}
	return (cost);
}

int solutionVRP::getViolations(int constraint) {
	int v = 0;
	int *load = new int [NVeh+1];	// load[i] = x iff vehicle i has a cumulative load of x
	
	for (int r=1; r<NVeh+1; r++) {	// compute vehicle loads
		int i = previous[r];
		load[r] = 0;
		while (i != r) {
			load[r] += demand[i];
			i=previous[i];
		} 
	}

	if(constraint == 1 || constraint == 0) for(int r=1; r<NVeh+1; r++) if (load[r] > Q) v += load[r] - Q;

	return (v);
}		


string& solutionVRP::toString() {
	ostringstream out;
	float *load = new float [NVeh+1];	// load[i] = x iff vehicle i has a cumulative load of x
	
	//for (int i=1; i<NVeh+N+1; i++)
	//	cout << "(i:" << i-NVeh << ",v:" << vehicle[i]-NVeh << "," << previous[i]-NVeh << "," 
	//			<< next[i]-NVeh << ",q:" << demand[i] << ",x:" << coordX[i] << ",y:" << coordY[i] << ") " << endl;
	for (int r=1; r<NVeh+1; r++) {	// compute vehicle loads
		int i = previous[r];
		load[r] = 0;
		while (i != r) {
			load[r] += demand[i]; 
			i=previous[i];
		} 
	}
	if (getViolations() > 0) out << "Infeasible ! ";
	out << "Cost=" << getCost() << " \n";
	for(int r=1; r<NVeh+1; r++) {
		out << "Route " << r << "(" << load[r] << "): D ";
		for(int i=next[r]; i!=r; i=next[i])
			out << i-NVeh << " ";
		out << "D\n";
	}
	static string str = ""; str = out.str();
	return (str);
}
