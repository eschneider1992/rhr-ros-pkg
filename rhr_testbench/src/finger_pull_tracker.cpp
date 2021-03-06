#include <ros/ros.h>
#include <std_msgs/Bool.h>

#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
using namespace std;


/**
This code tracks the pull count of the finger pull testbench and notes the cycles
It also records when failure occurs electrically
Eric Schneider, July 2014
**/


void blocked_callback(const std_msgs::Bool::ConstPtr& msg);
void cycle_done();
void connection_callback(const std_msgs::Bool::ConstPtr& msg);
void determine_previous_cycles(string cycle_file_address, string fail_file_address);
int read_past_cycles(string address);


// bool last_blocked = false;
bool connected = true;
bool ever_failed = false;

float start_time;
int cycle_start;
int cycle_counter = 0;
float cycle_hz = 20/24.0;		// When timed, it took approx. 24 seconds to complete 20 cycles
ofstream cycle_file;
ofstream fail_file;


// ==========================================================================================================================
// This block of code was used with the photogate. Decided the photogate wasn't worth using, because it missed cycles and
// was easily replaced by assuming the motor turned at a constant velocity

// // Called when the photogate data updates. If a gate is blocked and it wasn't previously, signals that a cycle finished
// void blocked_callback(const std_msgs::Bool::ConstPtr& msg) {
// 	if ((msg->data == true) && (last_blocked == false))
// 		cycle_done();
// 	if (msg->data == true)
// 		last_blocked = true;
// 	else
// 		last_blocked = false;
// }


// // Called when each cycle finishes, prints the cycle counter and connectivity state, both to ROS and the log file
// void cycle_done() {
// 	ROS_INFO("The test is at %d cycles. Is wire still connected: %d", ++cycle_counter, connected);
// 	cycle_file << '\n' << cycle_counter << " cycles completed, they took " << (ros::Time::now().toSec() - start_time) << " seconds";
// }
// ==========================================================================================================================



// Called when the connectivity data updates. The FIRST time that connectivity is false, this writes the failure to a log file. After that it does nothing
void connection_callback(const std_msgs::Bool::ConstPtr& msg) {
	if (msg->data == true)
		connected = true;
	else
		connected = false;

	if ((ever_failed == false) && (connected == false)) {
		ever_failed = true;
		ROS_WARN("The wire failed at %d cycles", cycle_counter);
		fail_file << "After " << cycle_counter << " cycles the wire connection has failed. It took " << (ros::Time::now().toSec() - start_time) << " seconds \n";
	}
}


// If the test is on a new finger, this function wipes the file locations and creates new files
// If the test is on a finger that has been tested previously, this code looks for the last cycle value written to a log file
// If the log file is missing but the finger is indicated as old, this code asks the user for the number of cycles that have been run
int determine_previous_cycles(string cycle_file_address, string fail_file_address, int cycle_counter) {
	char buffer[64];
	printf("Is this a new finger (no cycles run on it)? (y/n)");
	while (buffer[0] != 'y' && buffer[0] != 'Y' && buffer[0] != 'n' && buffer[0] != 'N') {
		fgets(buffer, 64, stdin);
		if (buffer[0] != 'y' && buffer[0] != 'Y' && buffer[0] != 'n' && buffer[0] != 'N')
			printf("........You need to enter y or n\n");
	}
	if (buffer[0] == 'n' || buffer[0] == 'N') {
		int past_cycles;
		if (past_cycles = read_past_cycles(cycle_file_address)) {
			cycle_counter += past_cycles;
			ROS_INFO("Found previous files indicating %d cycles had already past, cycle_counter reset to %d", past_cycles, cycle_counter);
		}
		else {
			printf("Couldn't find the saved data documenting past tests, how many cycles has this finger already done?\n");
			fgets(buffer, 64, stdin);
			past_cycles = atoi(buffer);
			printf("You indicated that %d cycles have already passed, adding that to the current run\n", past_cycles);
			cycle_counter+=past_cycles;
			ROS_INFO("User indicated %d cycles had already past, cycle counter reset to %d", past_cycles, cycle_counter);
		}
	}
	return cycle_counter;
}


// Extracts the last line of a file indicated by address, then tries to interpret the first word on that line as an integer
// The way the log files are set up, this should extract the previous number of cycle steps
int read_past_cycles(string address) {
	string buff;
	ifstream read_file(address.c_str());
	if (read_file.is_open()) {
		while ( getline (read_file, buff) ) {
			ROS_INFO("Loading old cycle data: \t%s", buff.c_str());
		}
		read_file.close();
	}
	else {
		ROS_INFO("There was no file to read past cycles from\n");
		return 0;
	}
	size_t space_pos = buff.find(" ");
	string clipped = buff.substr(0, space_pos);

	int result;
	try {
		result = atoi(clipped.c_str());
		ROS_INFO("Successfully read a previous cycle value of %d", result);
	}
	catch (int e) {
		ROS_INFO("The first characters in the line weren't integers, couldn't parse the number of previous cycles");
		return 0;
	}

	return result;
}


void cycle_done() {
	float elapsed = ros::Time::now().toSec() - start_time;
	cycle_counter = cycle_start + cycle_hz * elapsed;
	ROS_INFO("The test is at %d cycles. Is wire still connected: %d", cycle_counter, connected);
	cycle_file << '\n' << cycle_counter << " cycles completed, they took " << elapsed << " seconds";
}


int main(int argc, char **argv){
	ros::init(argc, argv, "finger_pull_tracker");
	ros::NodeHandle n;
	ros::Rate r(0.25);	// Every 4 seconds it will update the cycle count
	start_time = ros::Time::now().toSec();

	string cycle_file_address = "/home/emp/catkin_ws/src/rhr-ros-pkg/rhr_testbench/log/fp_cycle_counter.txt";
	string fail_file_address = "/home/emp/catkin_ws/src/rhr-ros-pkg/rhr_testbench/log/fp_connection_failed.txt";
	
	cycle_start = determine_previous_cycles(cycle_file_address, fail_file_address, cycle_counter);

	cycle_file.open(cycle_file_address.c_str(), ios::out|ios::trunc);
	fail_file.open(fail_file_address.c_str(), ios::out|ios::trunc);
	cycle_done();	// This preserves the previous cycle count if the run is stopped before any cycles happen

	// ros::Subscriber cycle_sub = n.subscribe("/is_blocked_1", 20, blocked_callback);
	ros::Subscriber connect_sub = n.subscribe("/is_connected", 20, connection_callback);

	printf("CYCLE HZ: %f\n", cycle_hz);

	while (ros::ok()){
		cycle_done();
		r.sleep();
	}

	cycle_file.close();
	fail_file.close();
	return 0;
}
