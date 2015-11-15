

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <wiringPi.h>
#include <time.h>
#include <signal.h>

#include "app_signal.h"
#include "rwalk_process.h"
#include "feature_processing.h"
#include "ipc_status_comm.h"
#include "feature_input.h"
#include "mindbx_lib.h"
#include "xml.h"

appconfig_t* app_config;

#define CONFIG_NAME "config/application_config.xml"

static unsigned char app_running = 0x01;
static char task_running = 0x00;

static rwalk_options_t rwalk_opts;
static feature_input_t feature_input_1;
static ipc_comm_t ipc_comm_1;
static feat_proc_t feature_proc_1;

void* train_player(void* param);
void* get_sample(void* param);
void run_testbench();

/**
 * which_config(int argc, char **argv)
 * @brief return which config to use
 * @param argc
 * @param argv
 * @return string of config
 */
inline char *which_config(int argc, char **argv)
{

	if (argc == 2) {
		return argv[1];
	} else {
		return CONFIG_NAME;
	}
}

/**
 * main(int argc, char **argv)
 * @brief test the mindbx_lib
 */
int main(int argc, char **argv){
	
	double decision_var_value = 0.0;
	double threshold = 0.0;
	int player_1_intensity;
	pthread_t thread;
	pthread_attr_t attr;
	clock_t start, end;
	double cpu_time_used;
	double fail_safe_counter = 0;

	/*Set up ctrl c signal handler*/
	(void)signal(SIGINT, ctrl_c_handler);

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	/*read the xml*/
	app_config = xml_initialize(which_config(argc, argv));
	
	/*configure the mind box*/
	setup_mindbx();
	
	/*configure the feature input*/
	feature_input_1.shm_key=7804;
	feature_input_1.sem_key=1234;
	init_feature_input(app_config->feature_source, &feature_input_1);
	
	/*configure the inter-process communication channel*/
	ipc_comm_1.sem_key=1234;
	ipc_comm_init(&ipc_comm_1);
	
	/*set LED strip to pairing mode*/
	set_led_strip_flash_state(WHITE, OFF, 1000);
	
	printf("App, waiting for HW\n");
	/*if required, wait for eeg hardware to be present*/
	if(app_config->eeg_hardware_required){
		if(!ipc_wait_for_harware(&ipc_comm_1)){
			exit(0);
		}
	}
	
	feature_proc_1.nb_train_samples = app_config->training_set_size;
	feature_proc_1.feature_input = &feature_input_1;
	init_feat_processing(&feature_proc_1);
	
	rwalk_opts.drift_rate_std = app_config->noise_std_dev;
	rwalk_opts.dt = app_config->time_period;
	init_rwalk_process(rwalk_opts);
	
	threshold = app_config->threshold;
	
	printf("App, entering app loop\n");
		
	/*while application is alive*/
	while(app_running){
	
		/*set LED strip to wait mode*/
		set_led_strip_flash_state(PINK, OFF, 500);
	
		/*wait for a coin*/
		wait_for_coin_insertion();
		
		/*set LED strip to train mode*/
		set_led_strip_flash_state(GREEN, OFF, 500);
		
		/*wait 3 seconds*/
		sleep(3);
		
		/*build training set*/
		printf("Building training set\n");
		//pthread_create(&thread, &attr, train_player, (void*) &feature_proc_1); 
		//pthread_join(thread,NULL);
		
		train_feat_processing(&feature_proc_1);
		
		printf("Training set acquisition, done!\n");
		
		/*reset random walk process*/
		reset_rwalk_process();
	
		task_running = 0x01;
		decision_var_value = 0;
		fail_safe_counter = 0;
		
		/*set LED strip to test mode*/
		set_led_strip_flash_state(BLUE, RED, 1100);

		/*wait 3 seconds*/
		sleep(3);
		
		printf("Task begin!\n");
		
		start = clock();
		
		/*run the test*/
		while(task_running){
		
			/*get a normalized sample*/
			//pthread_create(&thread, &attr, get_sample, (void*) &feature_proc_1); 
			//pthread_join(thread,NULL);
			
			get_normalized_sample(&feature_proc_1);
			
			/*push it to to noisy integrator*/
			decision_var_value = iterate_rwalk_process(feature_proc_1.sample);
			
			//if(get_selector_state()){
			//	fail_safe_counter += 0.333;
			//}
			
			decision_var_value += fail_safe_counter;
			
			printf("DV:%f\n",decision_var_value);
			
			/*update flash frequency*/
			set_led_strip_flash_state(BLUE, RED, ((threshold-decision_var_value)/threshold)*1000+100);
			
			end = clock();
			cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC * 1000;
			
			/*check if one of the stop conditions is met*/
			if(decision_var_value>threshold || app_config->test_duration < cpu_time_used){
				task_running = 0x00;
			}
			
		}
		
		if(decision_var_value>threshold){
			/*if we have a winner*/
			/*open the door*/
			open_door();
			set_led_strip_flash_state(BLUE, WHITE, 500);
			sleep(3);
		}else{
			set_led_strip_flash_state(RED, WHITE, 500);
			sleep(3);
		}
		
	}
	
	
	exit(0);
}

void cleanup_app(void){
	
	printf("mindbox_app->cleanup\n");
	fflush(stdout);
			
	ipc_comm_cleanup(&ipc_comm_1);
	clean_up_feat_processing(&feature_proc_1);
	
	reset_led_strip_flash_state();
	set_led_strip_color(OFF);
	exit(0);
}


void* train_player(void* param){
	train_feat_processing((feat_proc_t*)param);	
}

void* get_sample(void* param){
	get_normalized_sample((feat_proc_t*)param);
}
