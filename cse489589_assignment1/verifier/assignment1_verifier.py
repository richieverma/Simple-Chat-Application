import argparse

parser = argparse.ArgumentParser(description='CSE 489/589 PA1: Verifier')

requiredArgs = parser.add_argument_group('required named arguments')
requiredArgs.add_argument('-p', '--path', dest='path', type=str, nargs=1, help='path to your binary', required=True)

args = parser.parse_args()

def procStatus(pid):
    for line in open("/proc/%d/status" % pid).readlines():
        if line.startswith("State:"):
            return line.split(":",1)[1].strip().split(' ')[0]
    return None

if __name__ == "__main__":
	
	import sys
	import subprocess
	import time
	import os
	import socket
	import re
	
	def extractOutputSuccess(command, logfile_path):
		logfile = open(logfile_path, 'r')
		log_output = logfile.read()
		logfile.close()

		matches = re.compile('\['+command+':SUCCESS\]\\n(.*?)\['+command+':END\]\\n', re.DOTALL).search(log_output)
		
		if not matches: return "No output detected: Check format/syntax of output."
		return matches.group(1)

	s_or_c = 's'
	port = 4242

	print "Starting Application as "+s_or_c+" on port:"+str(port)

	#Application Startup
	print
	print "Application Startup ...",
	sys.stdout.flush()
	command = args.path[0]+" "+s_or_c+" "+str(port)
	process = subprocess.Popen(command, shell=True, stdout=open(os.devnull, 'w'), stderr=subprocess.STDOUT)
	time.sleep(2)
	status = procStatus(process.pid)
	if status == 'R' or status == 'S':
		os.system('kill -9 '+str(process.pid))
		print "PASS"
	else:
		print "FAIL"
		exit()

	logfile_path = os.path.dirname(args.path[0])+"/logs/assignment_log_"+socket.gethostname()
	
	#AUTHOR
	print
	print "AUTHOR ...",
	sys.stdout.flush()
	expect_command = "expect -f author.exp "+args.path[0]+" "+s_or_c+" "+str(port)
	process = subprocess.Popen(expect_command, shell=True, stdout=open(os.devnull, 'w'), stderr=subprocess.STDOUT)
	time.sleep(2)
	print "I got the following output:"
	print extractOutputSuccess("AUTHOR", logfile_path+"_"+str(port))

	#IP
	print
	print "IP ...",
	sys.stdout.flush()
	expect_command = "expect -f ip.exp "+args.path[0]+" "+s_or_c+" "+str(port)
	process = subprocess.Popen(expect_command, shell=True, stdout=open(os.devnull, 'w'), stderr=subprocess.STDOUT)
	time.sleep(3)
	print "I got the following output:"
	print extractOutputSuccess("IP", logfile_path+"_"+str(port))

	#PORT
	print
	print "PORT ...",
	sys.stdout.flush()
	expect_command = "expect -f port.exp "+args.path[0]+" "+s_or_c+" "+str(port)
	process = subprocess.Popen(expect_command, shell=True, stdout=open(os.devnull, 'w'), stderr=subprocess.STDOUT)
	time.sleep(3)
	print "I got the following output:"
	print extractOutputSuccess("PORT", logfile_path+"_"+str(port))

	#LIST
	print
	print "LIST ...",
	sys.stdout.flush()
	expect_command = "expect -f list_server.exp "+args.path[0]+" "+s_or_c+" "+str(port)
	server = subprocess.Popen(expect_command, shell=True, stdout=open(os.devnull, 'w'), stderr=subprocess.STDOUT)

	s_or_c = 'c'
	port = 1111
	expect_command = "expect -f list_client.exp "+args.path[0]+" "+s_or_c+" "+str(port)
	client_1 = subprocess.Popen(expect_command, shell=True, stdout=open(os.devnull, 'w'), stderr=subprocess.STDOUT)
	port = 1212
	expect_command = "expect -f list_client.exp "+args.path[0]+" "+s_or_c+" "+str(port)
	client_2 = subprocess.Popen(expect_command, shell=True, stdout=open(os.devnull, 'w'), stderr=subprocess.STDOUT)
	port = 1313
	expect_command = "expect -f list_client.exp "+args.path[0]+" "+s_or_c+" "+str(port)
	client_3 = subprocess.Popen(expect_command, shell=True, stdout=open(os.devnull, 'w'), stderr=subprocess.STDOUT)
	port = 1414
	expect_command = "expect -f list_client.exp "+args.path[0]+" "+s_or_c+" "+str(port)
	client_4 = subprocess.Popen(expect_command, shell=True, stdout=open(os.devnull, 'w'), stderr=subprocess.STDOUT)

	time.sleep(15)
	print "I got the following output:"
	print "-On Server-"
	print extractOutputSuccess("LIST", logfile_path+"_4242")
	print "-On Client 1-"
	print extractOutputSuccess("LIST", logfile_path+"_1111")
	print "-On Client 2-"
	print extractOutputSuccess("LIST", logfile_path+"_1212")
	print "-On Client 3-"
	print extractOutputSuccess("LIST", logfile_path+"_1313")
	print "-On Client 4-"
	print extractOutputSuccess("LIST", logfile_path+"_1414")

	os.system('kill -9 '+str(client_1.pid))
	os.system('kill -9 '+str(client_2.pid))
	os.system('kill -9 '+str(client_3.pid))
	os.system('kill -9 '+str(client_4.pid))
