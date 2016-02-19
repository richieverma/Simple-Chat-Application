#!/bin/bash
       
#Title           :assignment1_update_verifier.sh
#description     :This script will update the verifier scripts for assignment1.
#Author		     :Swetank Kumar Saha <swetankk@buffalo.edu>
#Version         :1.0
#====================================================================================

cd verifier
wget -r --no-parent -nH --cut-dirs=3 -R index.html http://ubwins.cse.buffalo.edu/cse-489_589/verifier/
cd ..