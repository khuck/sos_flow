#!/usr/bin/python 

import simplejson as json
from pprint import pprint
import argparse
import subprocess
import os
import shlex
import time
import sys, traceback

#filename = "5nodes.json"
filename = "2nodes.json"
commands = []
sos_root = ""

def parse_arguments():
    parser = argparse.ArgumentParser(description='Generate and execute a generic workflow example.')
    parser.add_argument('filename', metavar='filename', type=str, help='a JSON input config file')
    args = parser.parse_args()
    return args.filename

def generate_xml(data):
    header = """<?xml version="1.0"?>\n    <adios-config host-language="C">\n"""
    footer = """<buffer size-MB="20" allocate-time="now"/>\n</adios-config>\n"""
    group_data = """    <adios-group name="REPLACEME" coordination-communicator="comm" stats="On">
        <var name="NX" path="/scalar/dim" type="integer"/>
        <var name="NY" path="/scalar/dim" type="integer"/>
        <var name="NZ" path="/scalar/dim" type="integer"/>
        <var name="size" type="integer"/>
        <var name="rank" type="integer"/>
        <var name="offset" type="integer"/>
        <var name="size_y" type="integer"/>
        <var name="shutdown" type="integer"/>
        <global-bounds dimensions="/scalar/dim/NZ,size_y,/scalar/dim/NX" offsets="0,offset,0">
            <var name="var_2d_array" gwrite="t" type="double" dimensions="/scalar/dim/NZ,/scalar/dim/NY,/scalar/dim/NX"/>
        </global-bounds>
    </adios-group>\n"""
    group_method = """    <method group="REPLACEME" method="FLEXPATH">QUEUE_SIZE=4</method>\n"""
    f = open('arrays.xml', 'w')
    f.write(header)
    for node in data["nodes"]:
        for child in node["children"]:
            replacement = node["name"] + "_to_" + child
            f.write(group_data.replace("REPLACEME", replacement))
    for node in data["nodes"]:
        for child in node["children"]:
            replacement = node["name"] + "_to_" + child
            f.write(group_method.replace("REPLACEME", replacement))
    f.write(footer)
    f.close()

def generate_commands(data):
    commands = []
    global sos_root
    sos_root = data["sos_root"]
    for node in data["nodes"]:
        command = "mpirun -np " + node["mpi_ranks"] + " " + sos_root + "/bin/generic_node --name " + node["name"]
        if "iterations" in node:
            command = command + " --iterations " + str(node["iterations"])
        #command = command + " --iterations " + str(data["iterations"])
        for child in node["children"]:
            command = command + " --writeto " + child
        for parent in node["parents"]:
            command = command + " --readfrom " + parent
        commands.append(command)
    return commands
    
def execute_commands(commands, data):
    sos_root = data["sos_root"]
    sos_num_daemons = data["sos_num_daemons"]
    sos_cmd_port = data["sos_cmd_port"]
    sos_cmd_buffer_len = data["sos_cmd_buffer_len"]
    sos_num_dbs = data["sos_num_dbs"]
    sos_db_port = data["sos_db_port"]
    sos_db_buffer_len = data["sos_db_buffer_len"]
    sos_working_dir = data["sos_working_dir"]
    os.environ['SOS_ROOT'] = sos_root
    os.environ['SOS_CMD_PORT'] = sos_cmd_port
    os.environ['SOS_DB_PORT'] = sos_db_port
    daemon = sos_root + "/bin/sosd"
    arguments = "mpirun -np " + sos_num_daemons + " " + daemon + " --role SOS_ROLE_DAEMON --port " + sos_cmd_port + " --buffer_len " + sos_cmd_buffer_len + " --listen_backlog 10 --work_dir " + sos_working_dir + " : -np " + sos_num_dbs + " " +  daemon + " --role SOS_ROLE_DB --port " + sos_db_port + " --buffer_len " + sos_cmd_buffer_len + " --listen_backlog 10 --work_dir " + sos_working_dir 
    print arguments
    args = shlex.split(arguments)
    subprocess.Popen(args)
    time.sleep(1)
    index = 0
    for command in commands:
        index = index + 1
        print command
        args = shlex.split(command)
        if index == len(commands):
            subprocess.check_call(args)
        else:
            subprocess.Popen(args)
        time.sleep(0.1)

    # shut down
    print "Waiting for all processes to finish..."
    time.sleep(2)
    print "Stopping the database..."
    arguments = sos_root + "/bin/sosd_stop"
    args = shlex.split(arguments)
    subprocess.call(args)

def main():
    global sos_root
    filename = parse_arguments()
    with open(filename) as data_file:    
        data = json.load(data_file)
    #pprint(data)
    generate_xml(data)
    commands = generate_commands(data)
    try:
        execute_commands(commands, data)
    except:
        print "failed!"
        traceback.print_exc(file=sys.stderr)
        arguments = sos_root + "/bin/sosd_stop"
        args = shlex.split(arguments)
        subprocess.call(args)

if __name__ == "__main__":
    main()
