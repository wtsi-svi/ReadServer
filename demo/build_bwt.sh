#!/bin/bash

set -e


############################################################################################################################################################################################################################################
#	ONLY MAKE CHANGES HERE, DO NOT ALTER OTHER PARTS OF THE SCRIPT
############################################################################################################################################################################################################################################

	INST_SRC_DIR="<path>"    # This should be set to the absolute path to 'ReadServer' project directory created by 'git' (e.g. "/usr/local/ReadServer")

	INST_SMP=10        # Number of samples for which to build the ReadServer. Anything up to 50 should work. Two samples will finish roughly within 1hr using a single thread and lustre disks.
	INST_CLEANUP=1     # Control variable: "INST_CLEANUP"=="1" => script will delete the previous step once the next one is completed; "INST_CLEANUP"=="0" => script won't (this will lead to a large amount of data accumulating in the destination directory)
	INST_PTHREADS=1    # Number of parallel threads during most of the data processing steps. Should be between: (1*no.Cores) <= THREADS <= (2*no.Cores)

############################################################################################################################################################################################################################################





#==========================================================================================================================================================================================================================================#
#	COMMAND LINE PARAMETERS
#==========================================================================================================================================================================================================================================#

INST_RANGE=$1
INST_DST=$2

#==========================================================================================================================================================================================================================================#



#==========================================================================================================================================================================================================================================#
#	SOURCE DIRECTORY PATH CHECK
#==========================================================================================================================================================================================================================================#

if [[ "${INST_SRC_DIR}" == "<path>" ]] ; then
	echo -e "\nSource path has not been changed yet! Please edit 'build_bwt.sh' and insert the absolut path to the ReadServer GitHub repository (e.g. \"/usr/local/ReadServer\")!!\n"
	exit 99
fi

#==========================================================================================================================================================================================================================================#



#==========================================================================================================================================================================================================================================#
#	PATHS & VARIABLES
#==========================================================================================================================================================================================================================================#

INST_START_AT=1		# First step of the processing pipeline
INST_STOP_AT=10		# Last  step of the processing pipeline

INST_LIB_DIR="${INST_SRC_DIR}/libs/lib"
INST_BIN_DIR="${INST_SRC_DIR}/libs/bin"
INST_SCR_DIR="${INST_SRC_DIR}/scripts"

export "LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${INST_LIB_DIR}"


#- Display help in case no commandline parameters are specified
if [[ "${INST_RANGE}" == "" ]] && [[ "${INST_DST}" == "" ]] ; then
	echo -e "\nProgram:\tBuildBWT [DEMO] (Build a BWT ReadServer using a demonstration read set)"
	echo -e "Version:\t1.0"
	echo -e "\nUsage:\t./build_bwt.sh <start_step>-<end_step> [<path_to_destination_directory>]"
	
	echo -e "\nValid steps are:\n"
	echo -e "\tSTEP  0:\tRemove previous building attempts from destination directory (Make sure you really want that!!)"
	echo -e "\tSTEP  1:\tRetrieve sequencing data test set"
	echo -e "\tSTEP  2:\tConcatenate FASTQ files per sample"
	echo -e "\tSTEP  3:\tUse \"BFC\" for within-sample error correction"
	echo -e "\tSTEP  4:\tConvert error corrected FASTA files into files with: 1st line = READ; 2nd line = SAMPLE NAME and trim reads to maximum 100nt length"
	echo -e "\tSTEP  5:\tSplit processed files into smaller parts for parallel sorting"
	echo -e "\tSTEP  6:\tRLO (i.e. \"Reverse Lexicograpic Ordering\") sort sequences & convert sample names -> hash values"
	echo -e "\tSTEP  7:\tMerge RLO sorted reads"
	echo -e "\tSTEP  8:\tBuild BWT from RLO sorted & deduplicated read set & index afterwards"
	echo -e "\tSTEP  9:\tLoad reads (as keys) and sample names (as values) into RocksDB"
	echo -e "\tSTEP 10:\tMove all necessary data to server folder"
	echo -e "\tSTEP 11:\tConfigure server (This step is interactive! Run steps 1-10 first [e.g. on a Farm] and then run this step interactively)"
	echo -e "\n"
	
	exit 98
fi
#-


#- Checking range of steps to run
if [[ `echo "${INST_RANGE}" | awk --posix 'BEGIN{}{if($0~/^([[:digit:]]{1,2})\-([[:digit:]]{1,2})$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
	echo -e "\nNo valid range of steps specified! Please restart specifying a valid range of steps (\"./build_bwt.sh [1-11]-[1-11]\")\n"
	exit 97
else
	INST_START_AT=`echo "${INST_RANGE}" | sed -r 's/^([[:digit:]]{1,2})\-([[:digit:]]{1,2})$/\1/'`
	INST_STOP_AT=`echo "${INST_RANGE}" | sed -r 's/^([[:digit:]]{1,2})\-([[:digit:]]{1,2})$/\2/'`
	
	#- Are steps out of range?
	if [[ "${INST_START_AT}" -gt 11 ]] || [[ "${INST_STOP_AT}"  -gt 11 ]] ; then
		echo -e "\nOnly steps between 1-11 are valid! Please restart with valid steps!\n"
		exit 96
	fi
	#-
	
	#- Does STOP happen before START?
	if [[ "${INST_STOP_AT}" -lt "${INST_START_AT}" ]] ; then
		echo -e "\nLast step occurs before first step! First step always has to be lower than the last!\n"
		exit 95
	fi
	#-
fi
#-


#-	Checking demo server destination directory
if [[ "${INST_DST}" == "" ]] ; then
	INST_DST=$PWD
	echo "No server destination directory specified! Using: '${INST_DST}'"
	
	sleep 5s
else
	if [[ "${INST_DST}" == "." ]] ; then
		INST_DST=$PWD
	else
		if [[ `echo "${INST_DST}" | awk 'BEGIN{}{if($0~/^\//){print "1"}else{print "0"}}END{}'` -ne 1 ]] ; then
			echo -e "\nDestination directory has to be an absolute path!\n"
			exit 94
		else
			INST_DST=`echo "${INST_DST}" | sed -r 's/\/$//'`
			
			mkdir -p "${INST_DST}"
		fi
	fi
fi
#-


#- Checking whether necessary source files are present
if ! [[ -e "${INST_SRC_DIR}/demo/ng.3281-S2.csv" ]] ; then
	echo -e "\nFile \"ng.3281-S2.csv\" does not exist in \"demo\" directory! Check your \"ReadServer\" GitHub repository!\n"
	exit 93
fi

if ! [[ -e "${INST_SRC_DIR}/demo/permutations-3.txt" ]] ; then
	echo -e "\nFile \"permutations-3.txt\" does not exist in \"demo\" directory! Check your \"ReadServer\" GitHub repository!\n"
	exit 92
fi
#-


#- Generate sample list & hash for DemoServer sequencing read set in case they don't exist yet

if ! [[ -e "${INST_DST}/list_of_sample_names" ]] && [[ "${INST_START_AT}" -ne 11 ]] ; then
	grep "ERR360" "${INST_SRC_DIR}/demo/ng.3281-S2.csv" | awk -F ',' '{print $3}' | sort -u | head -n "${INST_SMP}" > "${INST_DST}/list_of_sample_names"
fi

if ! [[ -e "${INST_DST}/list_of_sample_hash" ]] && [[ "${INST_START_AT}" -ne 11 ]] ; then
	"${INST_BIN_DIR}/create_samples_hash" "${INST_DST}/list_of_sample_names" "${INST_DST}/list_of_sample_hash"
fi
#-

#==========================================================================================================================================================================================================================================#





#==========================================================================================================================================================================================================================================#
#	DEMO SERVER BUILDING STEPS
#==========================================================================================================================================================================================================================================#

cd "${INST_DST}"

############################################################################################################################################################################################################################################
#	STEP 00: REMOVE PREVIOUS BUILDING ATTEMPTS FROM DESTINATION DIRECTORY
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 0 ]] ; then

	echo -e "\n->\tStarting STEP_00 ...\n"

#..........................................................................................................................................................................................................................................#

	rm -Rf STEP_*
	rm -Rf SERVER
	rm -f list_of_sample_names
	rm -f list_of_sample_hash

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=4;
		
		#- Check whether all relevant files/directories were deleted
		if ! [[ -e "${INST_DST}/list_of_sample_names" ]] ; then chk=$(( chk - 1 )); fi
		if ! [[ -e "${INST_DST}/list_of_sample_hash"  ]]  ; then chk=$(( chk - 1 )); fi
		if [[ `ls | grep -F "STEP_"  | wc -l` -eq 0   ]]  ; then chk=$(( chk - 1 )); fi
		if [[ `ls | grep -F "SERVER" | wc -l` -eq 0   ]]  ; then chk=$(( chk - 1 )); fi
		#-
		
		#-- If not ...
		if [[ "${chk}" -ne 0 ]] ; then
			echo -e "\nSTEP_00 did not finish successfully!!! Please check and re-run!\n"
			exit 1
		else
			echo -e "\n<-\tSTEP_00 finished!\n"
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			grep "ERR360" "${INST_SRC_DIR}/demo/ng.3281-S2.csv" | awk -F ',' '{print $3}' | sort -u | head -n "${INST_SMP}" > "${INST_DST}/list_of_sample_names"
			"${INST_BIN_DIR}/create_samples_hash" "${INST_DST}/list_of_sample_names" "${INST_DST}/list_of_sample_hash"
			
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 01: RETRIEVE SEQUENCING DATA TEST SET
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 1 ]] ; then

	echo -e "\n->\tStarting STEP_01 ...\n"

	if [[ -d STEP_01 ]] ; then
		echo -en "STEP_01:\tStep has been run before! Delete previous attempt and run again (default: \"y\")? (y/n)\t"
		
		if read -t 5 reply ; then
			if [[ `echo "${reply}" | awk 'BEGIN{}{$0=tolower($0);if($0~/^(y|yes)$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
				echo "Stopped!"
				exit 1
			fi
		fi
		
		rm -Rf STEP_01;
		
		echo -e "\nDeleted!"
	fi

#..........................................................................................................................................................................................................................................#

	mkdir -p STEP_01 && cd STEP_01
	
	#- Generate commands: For each sample in the list, generate 2 URLs for its FASTQ files.
	while read sample
	do
		echo "wget -q ftp://ftp.sra.ebi.ac.uk/vol1/fastq/ERR360/$sample/${sample}_1.fastq.gz" >> tmp.commands
		echo "wget -q ftp://ftp.sra.ebi.ac.uk/vol1/fastq/ERR360/$sample/${sample}_2.fastq.gz" >> tmp.commands
	done < "../list_of_sample_names"
	#-
	
	#- Run commands
	cat tmp.commands | perl "${INST_SCR_DIR}/parallel_run.pl" --threads "${INST_PTHREADS}"
	#-
	
	cd ..

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=999;
	
	if [[ -e list_of_sample_names ]] ; then chk=`cat list_of_sample_names | wc -l`; fi
		
		#-- Check whether all FASTQ files were downloaded
		while read sample
		do
			if [[ -e "STEP_01/${sample}_1.fastq.gz" ]] && [[ -e "STEP_01/${sample}_2.fastq.gz" ]] ; then chk=$(( chk - 1 )); fi
		done < list_of_sample_names
		#--
		
		#-- If so, delete tmp commands
		if [[ "${chk}" -eq 0 ]] ; then
			rm -f STEP_01/tmp.commands
			
			echo -e "\n<-\tSTEP_01 finished!\n"
		else
			echo -e "\nSTEP_01 did not finish successfully!!! Please check and re-run!\n"
			exit 1
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 02: CONCATENATE FASTQ FILES PER SAMPLE
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 2 ]] ; then

	echo -e "\n->\tStarting STEP_02 ...\n"

	if ! [[ -d STEP_01 ]] || [[ -e STEP_01/tmp.commands ]] ; then
		echo -e "STEP_02:\tPrevious step is either missing or has not finished successfully (\"tmp.commands\" is still present)! Script stopped!"
		exit 2
	fi
	
	if [[ -d STEP_02 ]] ; then
		echo -en "STEP_02:\tStep has been run before! Delete previous attempt and run again (default: \"y\")? (y/n)\t"
		
		if read -t 5 reply ; then
			if [[ `echo "${reply}" | awk 'BEGIN{}{$0=tolower($0);if($0~/^(y|yes)$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
				echo "Stopped!"
				exit 2
			fi
		fi
		
		rm -Rf STEP_02;
		
		echo -e "\nDeleted!"
	fi

#..........................................................................................................................................................................................................................................#

	mkdir -p STEP_02
	
	#- Generate commands
	while read sample
	do
		echo "zcat ${INST_DST}/STEP_01/${sample}_1.fastq.gz ${INST_DST}/STEP_01/${sample}_2.fastq.gz | gzip -1 > ${INST_DST}/STEP_02/${sample}.fastq.gz" >> STEP_02/tmp.commands
	done < list_of_sample_names
	#-
	
	#- Run commands
	cat STEP_02/tmp.commands | perl "${INST_SCR_DIR}/parallel_run.pl" --threads "${INST_PTHREADS}"
	#-

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=999;
	
	if [[ -e list_of_sample_names ]] ; then chk=`cat list_of_sample_names | wc -l`; fi
		
		#-- Check whether all FASTQ files were successfully concatenated
		while read sample
		do
			if [[ -e "STEP_02/${sample}.fastq.gz" ]] && [[ -s "STEP_02/${sample}.fastq.gz" ]] ; then chk=$(( chk - 1 )); fi
		done < list_of_sample_names
		#--
		
		#-- If so, delete original FASTQs and tmp commands
		if [[ "${chk}" -eq 0 ]] ; then
			if [[ ${INST_CLEANUP} -eq 1 ]] ; then rm -Rf STEP_01; fi
			
			rm -f STEP_02/tmp.commands
			
			echo -e "\n<-\tSTEP_02 finished!\n"
		else
			echo -e "\nSTEP_02 did not finish successfully!!! Please check and re-run!\n"
			exit 2
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 03: USE "BFC" FOR WITHIN-SAMPLE ERROR CORRECTION
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 3 ]] ; then

	echo -e "\n->\tStarting STEP_03 ...\n"

	if ! [[ -d STEP_02 ]] || [[ -e STEP_02/tmp.commands ]] ; then
		echo -e "STEP_03:\tPrevious step is either missing or has not finished successfully (\"tmp.commands\" is still present)! Script stopped!"
		exit 3
	fi
	
	if [[ -d STEP_03 ]] ; then
		echo -en "STEP_03:\tStep has been run before! Delete previous attempt and run again (default: \"y\")? (y/n)\t"
		
		if read -t 5 reply ; then
			if [[ `echo "${reply}" | awk 'BEGIN{}{$0=tolower($0);if($0~/^(y|yes)$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
				echo "Stopped!"
				exit 3
			fi
		fi
		
		rm -Rf STEP_03;
		
		echo -e "\nDeleted!"
	fi

#..........................................................................................................................................................................................................................................#

	mkdir -p STEP_03
	
	#- Generate commands
	while read sample
	do
		echo "${INST_SRC_DIR}/submodules/bfc/bfc -s 5m -t 4 -Q ${INST_DST}/STEP_02/${sample}.fastq.gz | gzip -1 > ${INST_DST}/STEP_03/${sample}.ec.fasta.gz" >> STEP_03/tmp.commands
	done < list_of_sample_names
	#-
	
	#- Run commands
	cat STEP_03/tmp.commands | perl "${INST_SCR_DIR}/parallel_run.pl" --threads "${INST_PTHREADS}"
	#-

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=999;
	
	if [[ -e list_of_sample_names ]] ; then chk=`cat list_of_sample_names | wc -l`; fi
		
		#-- Check whether all FASTQ files were successfully error corrected
		while read sample
		do
			if [[ -e "STEP_03/${sample}.ec.fasta.gz" ]] && [[ -s "STEP_03/${sample}.ec.fasta.gz" ]] ; then chk=$(( chk - 1 )); fi
		done < list_of_sample_names
		#--
		
		#-- If so, delete concatenated FASTQs and tmp commands
		if [[ "${chk}" -eq 0 ]] ; then
			if [[ ${INST_CLEANUP} -eq 1 ]] ; then rm -Rf STEP_02; fi
			
			rm -f STEP_03/tmp.commands
			
			echo -e "\n<-\tSTEP_03 finished!\n"
		else
			echo -e "\nSTEP_03 did not finish successfully!!! Please check and re-run!\n"
			exit 3
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 04: CONVERT ERROR CORRECTED FASTA FILES INTO FILES WITH: 1st LINE = READ; 2nd LINE = SAMPLE NAME AND TRIM READS TO MAXIMUM 100nt LENGTH
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 4 ]] ; then

	echo -e "\n->\tStarting STEP_04 ...\n"

	if ! [[ -d STEP_03 ]] || [[ -e STEP_03/tmp.commands ]] ; then
		echo -e "STEP_04:\tPrevious step is either missing or has not finished successfully (\"tmp.commands\" is still present)! Script stopped!"
		exit 4
	fi
	
	if [[ -d STEP_04 ]] ; then
		echo -en "STEP_04:\tStep has been run before! Delete previous attempt and run again (default: \"y\")? (y/n)\t"
		
		if read -t 5 reply ; then
			if [[ `echo "${reply}" | awk 'BEGIN{}{$0=tolower($0);if($0~/^(y|yes)$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
				echo "Stopped!"
				exit 4
			fi
		fi
		
		rm -Rf STEP_04;
		
		echo -e "\nDeleted!"
	fi

#..........................................................................................................................................................................................................................................#

	mkdir -p STEP_04
	
	#- Generate commands
	while read sample
	do
		echo "zcat ${INST_DST}/STEP_03/${sample}.ec.fasta.gz | sed 'N;s/\n/\t/' | awk -F \"[[:blank:]]+\" 'BEGIN{}{sub(/^>/,\"\",\$1);sub(/\..+\$/,\"\",\$1);\$3=substr(\$3,1,100);print \$3\"\n\"\$1}END{}' > ${INST_DST}/STEP_04/${sample}.ec.prcd" >> STEP_04/tmp.commands
	done < list_of_sample_names
	#-
	
	#- Run commands
	cat STEP_04/tmp.commands | perl "${INST_SCR_DIR}/parallel_run.pl" --threads "${INST_PTHREADS}"
	#-

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=999;
	
	if [[ -e list_of_sample_names ]] ; then chk=`cat list_of_sample_names | wc -l`; fi
		
		#-- Check whether all FASTA files were successfully processed
		while read sample
		do
			if [[ -e "STEP_04/${sample}.ec.prcd" ]] && [[ -s "STEP_04/${sample}.ec.prcd" ]] ; then chk=$(( chk - 1 )); fi
		done < list_of_sample_names
		#--
		
		#-- If so, delete error corrected FASTAs and tmp commands
		if [[ "${chk}" -eq 0 ]] ; then
			if [[ ${INST_CLEANUP} -eq 1 ]] ; then rm -Rf STEP_03; fi
			
			rm -f STEP_04/tmp.commands
			
			echo -e "\n<-\tSTEP_04 finished!\n"
		else
			echo -e "\nSTEP_04 did not finish successfully!!! Please check and re-run!\n"
			exit 4
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 05: SPLIT PROCESSED FILES INTO SMALLER PARTS FOR PARALLEL SORTING
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 5 ]] ; then

	echo -e "\n->\tStarting STEP_05 ...\n"

	if ! [[ -d STEP_04 ]] || [[ -e STEP_04/tmp.commands ]] ; then
		echo -e "STEP_05:\tPrevious step is either missing or has not finished successfully (\"tmp.commands\" is still present)! Script stopped!"
		exit 5
	fi
	
	if [[ -d STEP_05 ]] ; then
		echo -en "STEP_05:\tStep has been run before! Delete previous attempt and run again (default: \"y\")? (y/n)\t"
		
		if read -t 5 reply ; then
			if [[ `echo "${reply}" | awk 'BEGIN{}{$0=tolower($0);if($0~/^(y|yes)$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
				echo "Stopped!"
				exit 5
			fi
		fi
		
		rm -Rf STEP_05;
		
		echo -e "\nDeleted!"
	fi

#..........................................................................................................................................................................................................................................#

	mkdir -p STEP_05
	
	#- Generate commands
	while read sample
	do
		echo "split --lines=1000000 -a 4 ${INST_DST}/STEP_04/${sample}.ec.prcd ${INST_DST}/STEP_05/${sample}.ec.prcd.part" >> STEP_05/tmp.commands
	done < list_of_sample_names
	#-
	
	#- Run commands
	cat STEP_05/tmp.commands | perl "${INST_SCR_DIR}/parallel_run.pl" --threads "${INST_PTHREADS}"
	#-

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=999;
	
	if [[ -e list_of_sample_names ]] ; then chk=`cat list_of_sample_names | wc -l`; fi
		
		#-- Check whether all processed files were successfully split and are complete (i.e. total number of lines in all parts together has to be the same as in the source file)
		while read sample
		do
			lines=0
			
			for i in `ls STEP_05/${sample}.ec.prcd.part*` ; do lines=$(( lines + `cat "${i}" | wc -l` )); done
			
			if [[ ${lines} -eq `cat "STEP_04/${sample}.ec.prcd" | wc -l` ]] ; then chk=$(( chk - 1 )); fi
		done < list_of_sample_names
		#--
		
		#-- If so, delete source files and tmp commands
		if [[ "${chk}" -eq 0 ]] ; then
			if [[ ${INST_CLEANUP} -eq 1 ]] ; then rm -Rf STEP_04; fi
			
			rm -f STEP_05/tmp.commands
			
			echo -e "\n<-\tSTEP_05 finished!\n"
		else
			echo -e "\nSTEP_05 did not finish successfully!!! Please check and re-run!\n"
			exit 5
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 06: RLO (i.e. "Reverse Lexicograpic Ordering") SORT SEQUENCES & CONVERT SAMPLE NAMES -> HASH VALUES
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 6 ]] ; then

	echo -e "\n->\tStarting STEP_06 ...\n"

	if ! [[ -d STEP_05 ]] || [[ -e STEP_05/tmp.commands ]] ; then
		echo -e "STEP_06:\tPrevious step is either missing or has not finished successfully (\"tmp.commands\" is still present)! Script stopped!"
		exit 6
	fi
	
	if [[ -d STEP_06 ]] ; then
		echo -en "STEP_06:\tStep has been run before! Delete previous attempt and run again (default: \"y\")? (y/n)\t"
		
		if read -t 5 reply ; then
			if [[ `echo "${reply}" | awk 'BEGIN{}{$0=tolower($0);if($0~/^(y|yes)$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
				echo "Stopped!"
				exit 6
			fi
		fi
		
		rm -Rf STEP_06;
		
		echo -e "\nDeleted!"
	fi

#..........................................................................................................................................................................................................................................#

	mkdir -p STEP_06
	
	#- Generate temporary list of split files for sorting commands
	ls STEP_05/*.part* | sed -r 's/^[^\/]+\///' > STEP_05/tmp.list_of_parts
	#-
	
	#- Generate commands
	while read file
	do
		echo "${INST_BIN_DIR}/rlosort_seq_and_convert_sample_names ${INST_DST}/STEP_05/${file} ${INST_DST}/list_of_sample_hash ${INST_DST}/STEP_06/${file}.rlosorted" >> STEP_06/tmp.commands
	done < STEP_05/tmp.list_of_parts
	#-
	
	#- Run commands
	cat STEP_06/tmp.commands | perl "${INST_SCR_DIR}/parallel_run.pl" --threads "${INST_PTHREADS}"
	#-

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=999;
	
	if [[ -e STEP_05/tmp.list_of_parts ]] ; then chk=`cat STEP_05/tmp.list_of_parts | wc -l`; fi
		
		#-- Check whether there is an RLO sorted output for every input file
		while read file
		do
			if [[ -e "STEP_06/${file}.rlosorted" ]] && [[ -s "STEP_06/${file}.rlosorted" ]] ; then chk=$(( chk - 1 )); fi
		done < STEP_05/tmp.list_of_parts
		#--
		
		#-- If so, delete split files and tmp commands
		if [[ "${chk}" -eq 0 ]] ; then
			rm -f STEP_05/tmp.list_of_parts
			rm -f STEP_06/tmp.commands
			
			if [[ ${INST_CLEANUP} -eq 1 ]] ; then rm -Rf STEP_05; fi
			
			echo -e "\n<-\tSTEP_06 finished!\n"
		else
			echo -e "\nSTEP_06 did not finish successfully!!! Please check and re-run!\n"
			exit 6
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 07: MERGE RLO SORTED READS
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 7 ]] ; then

	echo -e "\n->\tStarting STEP_07 ...\n"

	if ! [[ -d STEP_06 ]] || [[ -e STEP_06/tmp.commands ]] ; then
		echo -e "STEP_07:\tPrevious step is either missing or has not finished successfully (\"tmp.commands\" is still present)! Script stopped!"
		exit 7
	fi
	
	if [[ -d STEP_07 ]] ; then
		echo -en "STEP_07:\tStep has been run before! Delete previous attempt and run again (default: \"y\")? (y/n)\t"
		
		if read -t 5 reply ; then
			if [[ `echo "${reply}" | awk 'BEGIN{}{$0=tolower($0);if($0~/^(y|yes)$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
				echo "Stopped!"
				exit 7
			fi
		fi
		
		rm -Rf STEP_07;
		
		echo -e "\nDeleted!"
	fi

#..........................................................................................................................................................................................................................................#

	mkdir -p STEP_07
	mkdir -p STEP_07/SOURCE
	mkdir -p STEP_07/SINK
	
	cp STEP_06/*.rlosorted STEP_07/SOURCE
	
	while [[ `ls STEP_07/SOURCE/* | wc -l` -ne 1 ]] ;
	do
		declare -a files=(`ls STEP_07/SOURCE/* | sed -r 's/^.+\///'`)
		count="${#files[@]}"
		
		#- Test and cleanup of previous round
		if [[ "${count}" -lt 1 ]] ; then
			echo -e "STEP_07:\tData merging step went wrong, no files left! Script stopped!!"
			exit 7;
		else
			rm -f STEP_07/tmp.commands
		fi
		#-
		
		#- Make sure number of files in SOURCE is even
		if [[ $(( count % 2 )) -ne 0 ]] ; then
			mv -v "STEP_07/SOURCE/${files[0]}" STEP_07/SINK/MERGED.0
			files=(${files[@]:1})
			count="${#files[@]}"
		fi
		#-
		
		#- Generate commands
		for i in `seq 1 $(( count / 2 ))` ;
		do
			idA=$(( i - 1 ))
			idB=$(( idA + (count / 2) ))
			
			echo "${INST_BIN_DIR}/merge_rlosorted_reads ${INST_DST}/STEP_07/SOURCE/${files[${idA}]} ${INST_DST}/STEP_07/SOURCE/${files[${idB}]} ${INST_DST}/STEP_07/SINK/MERGED.${i}" >> STEP_07/tmp.commands
		done
		#-
		
		#- Run commands
		cat STEP_07/tmp.commands | perl "${INST_SCR_DIR}/parallel_run.pl" --threads "${INST_PTHREADS}"
		#-
		
		#- Reset SOURCE & SINK directories for next merging round
		rm -f STEP_07/SOURCE/*
		mv -v STEP_07/SINK/* STEP_07/SOURCE/
		#-
		
		unset files
	done

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=1;
		
		#-- Check whether final merged (and deduplicated??) file exists
		if [[ -e "STEP_07/SOURCE/MERGED.1" ]] && [[ -s "STEP_07/SOURCE/MERGED.1" ]] ; then chk=$(( chk - 1 )); fi
		#--
		
		#-- If so, delete RLO sorted input files and tmp commands
		if [[ "${chk}" -eq 0 ]] ; then
			mv STEP_07/SOURCE/MERGED.1 STEP_07/merged.rlosorted.reads
			
			cd STEP_07
			rm -Rf SOURCE
			rm -Rf SINK
			cd ..
			
			rm -f STEP_07/tmp.commands
			
			if [[ ${INST_CLEANUP} -eq 1 ]] ; then rm -Rf STEP_06; fi
			
			echo -e "\n<-\tSTEP_07 finished!\n"
		else
			echo -e "\nSTEP_07 did not finish successfully!!! Please check and re-run!\n"
			exit 7
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 08: BUILD BWT FROM RLO SORTED & DEDUPLICATED READ SET AND INDEX AFTERWARDS
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 8 ]] ; then

	echo -e "\n->\tStarting STEP_08 ...\n"

	if ! [[ -d STEP_07 ]] || [[ -e STEP_07/tmp.commands ]] ; then
		echo -e "STEP_08:\tPrevious step is either missing or has not finished successfully (\"tmp.commands\" is still present)! Script stopped!"
		exit 8
	fi
	
	if [[ -d STEP_08 ]] ; then
		echo -en "STEP_08:\tStep has been run before! Delete previous attempt and run again (default: \"y\")? (y/n)\t"
		
		if read -t 5 reply ; then
			if [[ `echo "${reply}" | awk 'BEGIN{}{$0=tolower($0);if($0~/^(y|yes)$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
				echo "Stopped!"
				exit 8
			fi
		fi
		
		rm -Rf STEP_08;
		
		echo -e "\nDeleted!"
	fi

#..........................................................................................................................................................................................................................................#

	mkdir -p STEP_08
	
	echo -en "Read extraction  (started)  :\t"`date +'%a %b %d %T %Z %Y'`"\n"
	awk 'BEGIN{c=0}{if((NR % 2)!=0 && $0!~/N/){++c;print ">"c"\n"$0}}END{}' STEP_07/merged.rlosorted.reads > STEP_08/final.fa
	echo -en "Read extraction  (finished) :\t"`date +'%a %b %d %T %Z %Y'`"\n\n"
	
	echo -en "BWT construction (started)  :\t"`date +'%a %b %d %T %Z %Y'`"\n"
	"${INST_BIN_DIR}/sga" index -a ropebwt -t 4 --no-reverse --prefix="${INST_DST}/STEP_08/final" STEP_08/final.fa
	echo -en "BWT construction (finished) :\t"`date +'%a %b %d %T %Z %Y'`"\n\n"
	
	echo -en "Indexing BWT     (started)  :\t"`date +'%a %b %d %T %Z %Y'`"\n"
	"${INST_BIN_DIR}/index_rlebwt" "${INST_DST}/STEP_08/final.bwt"
	echo -en "Indexing BWT     (finished) :\t"`date +'%a %b %d %T %Z %Y'`"\n\n"

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=2;
		
		#-- Check whether BWT was built
		if [[ -e "STEP_08/final.bwt" ]] && [[ -s "STEP_08/final.bwt" ]] ; then chk=$(( chk - 1 )); fi
		if [[ -e "STEP_08/final.bwt.bpi2" ]] && [[ -s "STEP_08/final.bwt.bpi2" ]] ; then chk=$(( chk - 1 )); fi
		#--
		
		#-- If so, delete the intermediate filtered FASTA file and tmp commands
		if [[ "${chk}" -eq 0 ]] ; then
			rm -f STEP_08/final.fa
			
			echo -e "\n<-\tSTEP_08 finished!\n"
		else
			echo -e "\nSTEP_08 did not finish successfully!!! Please check and re-run!\n"
			exit 8
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 09: LOAD READS (AS KEYS) AND IDs (AS VALUES) INTO RocksDB
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 9 ]] ; then

	echo -e "\n->\tStarting STEP_09 ...\n"

	if ! [[ -d STEP_07 ]] || [[ -e STEP_07/tmp.commands ]] ; then
		echo -e "STEP_09:\tPrevious step is either missing or has not finished successfully (\"tmp.commands\" is still present)! Script stopped!"
		exit 9
	fi
	
	if [[ -d STEP_09 ]] ; then
		echo -en "STEP_09:\tStep has been run before! Delete previous attempt and run again (default: \"y\")? (y/n)\t"
		
		if read -t 5 reply ; then
			if [[ `echo "${reply}" | awk 'BEGIN{}{$0=tolower($0);if($0~/^(y|yes)$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
				echo "Stopped!"
				exit 9
			fi
		fi
		
		rm -Rf STEP_09;
		
		echo -e "\nDeleted!"
	fi

#..........................................................................................................................................................................................................................................#

	mkdir -p STEP_09
	
	#- Generate commands
	while read permutation
	do
		echo "${INST_BIN_DIR}/load_data_into_rocksdb ${INST_DST}/STEP_07/merged.rlosorted.reads ${INST_DST}/STEP_09/${permutation}.rocksdb ${permutation}" >> STEP_09/tmp.commands
	done < "${INST_SRC_DIR}/demo/permutations-3.txt"
	#-
	
	#- Run commands
	cat STEP_09/tmp.commands | perl "${INST_SCR_DIR}/parallel_run.pl" --threads "${INST_PTHREADS}"
	#-

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=999;
	
	if [[ -e "${INST_SRC_DIR}/demo/permutations-3.txt" ]] ; then chk=`cat "${INST_SRC_DIR}/demo/permutations-3.txt" | wc -l`; fi
		
		#-- Check whether RocksDBs were built
		while read permutation
		do
			if [[ -d "STEP_09/${permutation}.rocksdb" ]] ; then chk=$(( chk - 1 )); fi
		done < "${INST_SRC_DIR}/demo/permutations-3.txt"
		#--
		
		#-- If so, delete merged read file and tmp commands
		if [[ "${chk}" -eq 0 ]] ; then
			rm -f STEP_09/tmp.commands
			
			if [[ ${INST_CLEANUP} -eq 1 ]] ; then rm -Rf STEP_07; fi
			
			echo -e "\n<-\tSTEP_09 finished!\n"
		else
			echo -e "\nSTEP_09 did not finish successfully!!! Please check and re-run!\n"
			exit 9
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 10: MOVE ALL NECESSARY DATA TO SERVER FOLDER
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 10 ]] ; then

	echo -e "\n->\tStarting STEP_10 ...\n"

	if ! [[ -d STEP_09 ]] || [[ -e STEP_09/tmp.commands ]] ; then
		echo -e "STEP_10:\tPrevious step is either missing or has not finished successfully (\"tmp.commands\" is still present)! Script stopped!"
		exit 10
	fi
	
	if [[ -d SERVER ]] ; then
		echo -en "STEP_10:\tStep has been run before! Delete previous attempt and run again (default: \"y\")? (y/n)\t"
		
		if read -t 5 reply ; then
			if [[ `echo "${reply}" | awk 'BEGIN{}{$0=tolower($0);if($0~/^(y|yes)$/){print "1"}else{print "0"}}END{}'` -eq 0 ]] ; then
				echo "Stopped!"
				exit 10
			fi
		fi
		
		rm -Rf SERVER;
		
		echo -e "\nDeleted!"
	fi

#..........................................................................................................................................................................................................................................#

	mkdir -p SERVER/files SERVER/bwt SERVER/rocksdbs SERVER/logs
	
	cp list_of_sample_names list_of_sample_hash SERVER/files
	mv STEP_08/final.sai STEP_08/final.bwt STEP_08/final.bwt.bpi2 SERVER/bwt
	mv STEP_09/*.rocksdb SERVER/rocksdbs

#..........................................................................................................................................................................................................................................#

	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# FINISHING AND CLEANUP:
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	chk=999;
	
	if [[ -e "${INST_SRC_DIR}/demo/permutations-3.txt" ]] ; then chk=$(( `cat "${INST_SRC_DIR}/demo/permutations-3.txt" | wc -l` + 2 )); fi
		
		#-- Check whether BWT was moved
		if [[ -e SERVER/bwt/final.bwt ]] && [[ -s SERVER/bwt/final.bwt ]] ; then chk=$(( chk - 1 )); fi
		if [[ -e SERVER/bwt/final.bwt.bpi2 ]] && [[ -s SERVER/bwt/final.bwt.bpi2 ]] ; then chk=$(( chk - 1 )); fi
		#-
		
		#-- Check whether RocksDBs were moved
		while read permutation
		do
			if [[ -d "SERVER/rocksdbs/${permutation}.rocksdb" ]] ; then chk=$(( chk - 1 )); fi
		done < "${INST_SRC_DIR}/demo/permutations-3.txt"
		#--
		
		#-- If so, delete error corrected FASTQs and tmp commands
		if [[ "${chk}" -eq 0 ]] ; then
			rm -Rf STEP_08
			rm -Rf STEP_09
			
			if [[ ${INST_CLEANUP} -eq 1 ]] ; then
				rm -Rf list_of_sample_names
				rm -Rf list_of_sample_hash
			fi
			
			echo -e "\n<-\tSTEP_10 finished!\n"
		else
			echo -e "\nSTEP_10 did not finish successfully!!! Please check and re-run!\n"
			exit 10
		fi
		#--
		
	#---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################



############################################################################################################################################################################################################################################
#	STEP 11: CONFIGURE SERVER
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
if [[ "${INST_START_AT}" -eq 11 ]] ; then

	if ! [[ -d SERVER ]] ; then
		echo -e "STEP_11:\tPrevious step is missing! Script stopped!"
		exit 11
	fi

#..........................................................................................................................................................................................................................................#

	echo -en "Please specify the IP address for the machine supposed to host the server __ :\t"
	read READ_SERVER_IP
	
	echo -en "Please specify the main port for receiving web queries _____________________ :\t"
	read WEB_PORT
	
	echo -en "Please specify the port for count & read queries ___________________________ :\t"
	read COUNTS_READS_QUERY_PORT
	
	echo -en "Please specify the port for sample queries _________________________________ :\t"
	read SAMPLES_QUERY_PORT
	
	echo -en "Please specify the port for count results __________________________________ :\t"
	read COUNTS_RESULT_PORT
	
	echo -en "Please specify the port for read & sample results __________________________ :\t"
	read READS_SAMPLES_RESULT_PORT
	
	if [[ `echo -e "${WEB_PORT}\n${COUNTS_READS_QUERY_PORT}\n${SAMPLES_QUERY_PORT}\n${COUNTS_RESULT_PORT}\n${READS_SAMPLES_RESULT_PORT}" | sort -u | wc -l` -ne 5 ]] ; then
		echo "Ports have to be unique! Please re-run the configuration step!"
		exit 11
	fi
	
	cat ${INST_SRC_DIR}/demo/TEMPLATE.server.cfg           | awk -v vRSI="${READ_SERVER_IP}" -v vWP="${WEB_PORT}" -v vCRQP="${COUNTS_READS_QUERY_PORT}" -v vSQP="${SAMPLES_QUERY_PORT}" -v vCRP="${COUNTS_RESULT_PORT}" -v vRSRP="${READS_SAMPLES_RESULT_PORT}" -v vID="${INST_DST}" -v vIBD="${INST_BIN_DIR}" 'BEGIN{}{gsub(/READ_SERVER_IP/,vRSI,$0);gsub(/WEB_PORT/,vWP,$0);gsub(/COUNTS_READS_QUERY_PORT/,vCRQP,$0);gsub(/SAMPLES_QUERY_PORT/,vSQP,$0);gsub(/COUNTS_RESULT_PORT/,vCRP,$0);gsub(/READS_SAMPLES_RESULT_PORT/,vRSRP,$0);gsub(/INST_DST/,vID,$0);gsub(/INST_BIN_DIR/,vIBD,$0);print $0;}END{}' > ${INST_DST}/SERVER/server.cfg
	cat ${INST_SRC_DIR}/demo/TEMPLATE.service.cfg          | awk -v vRSI="${READ_SERVER_IP}" -v vWP="${WEB_PORT}" -v vCRQP="${COUNTS_READS_QUERY_PORT}" -v vSQP="${SAMPLES_QUERY_PORT}" -v vCRP="${COUNTS_RESULT_PORT}" -v vRSRP="${READS_SAMPLES_RESULT_PORT}" -v vID="${INST_DST}" -v vIBD="${INST_BIN_DIR}" 'BEGIN{}{gsub(/READ_SERVER_IP/,vRSI,$0);gsub(/WEB_PORT/,vWP,$0);gsub(/COUNTS_READS_QUERY_PORT/,vCRQP,$0);gsub(/SAMPLES_QUERY_PORT/,vSQP,$0);gsub(/COUNTS_RESULT_PORT/,vCRP,$0);gsub(/READS_SAMPLES_RESULT_PORT/,vRSRP,$0);gsub(/INST_DST/,vID,$0);gsub(/INST_BIN_DIR/,vIBD,$0);print $0;}END{}' > ${INST_DST}/SERVER/service.cfg
	cat ${INST_SRC_DIR}/demo/TEMPLATE.service_samples.cfg  | awk -v vRSI="${READ_SERVER_IP}" -v vWP="${WEB_PORT}" -v vCRQP="${COUNTS_READS_QUERY_PORT}" -v vSQP="${SAMPLES_QUERY_PORT}" -v vCRP="${COUNTS_RESULT_PORT}" -v vRSRP="${READS_SAMPLES_RESULT_PORT}" -v vID="${INST_DST}" -v vIBD="${INST_BIN_DIR}" 'BEGIN{}{gsub(/READ_SERVER_IP/,vRSI,$0);gsub(/WEB_PORT/,vWP,$0);gsub(/COUNTS_READS_QUERY_PORT/,vCRQP,$0);gsub(/SAMPLES_QUERY_PORT/,vSQP,$0);gsub(/COUNTS_RESULT_PORT/,vCRP,$0);gsub(/READS_SAMPLES_RESULT_PORT/,vRSRP,$0);gsub(/INST_DST/,vID,$0);gsub(/INST_BIN_DIR/,vIBD,$0);print $0;}END{}' > ${INST_DST}/SERVER/service_samples.cfg
	cat ${INST_SRC_DIR}/demo/TEMPLATE.start_demo_server.sh | awk -v vRSI="${READ_SERVER_IP}" -v vWP="${WEB_PORT}" -v vCRQP="${COUNTS_READS_QUERY_PORT}" -v vSQP="${SAMPLES_QUERY_PORT}" -v vCRP="${COUNTS_RESULT_PORT}" -v vRSRP="${READS_SAMPLES_RESULT_PORT}" -v vID="${INST_DST}" -v vIBD="${INST_BIN_DIR}" 'BEGIN{}{gsub(/READ_SERVER_IP/,vRSI,$0);gsub(/WEB_PORT/,vWP,$0);gsub(/COUNTS_READS_QUERY_PORT/,vCRQP,$0);gsub(/SAMPLES_QUERY_PORT/,vSQP,$0);gsub(/COUNTS_RESULT_PORT/,vCRP,$0);gsub(/READS_SAMPLES_RESULT_PORT/,vRSRP,$0);gsub(/INST_DST/,vID,$0);gsub(/INST_BIN_DIR/,vIBD,$0);print $0;}END{}' > ${INST_DST}/SERVER/start_demo_server.sh
	cat ${INST_SRC_DIR}/demo/TEMPLATE.exit_demo_server.sh  | awk -v vRSI="${READ_SERVER_IP}" -v vWP="${WEB_PORT}" -v vCRQP="${COUNTS_READS_QUERY_PORT}" -v vSQP="${SAMPLES_QUERY_PORT}" -v vCRP="${COUNTS_RESULT_PORT}" -v vRSRP="${READS_SAMPLES_RESULT_PORT}" -v vID="${INST_DST}" -v vIBD="${INST_BIN_DIR}" 'BEGIN{}{gsub(/READ_SERVER_IP/,vRSI,$0);gsub(/WEB_PORT/,vWP,$0);gsub(/COUNTS_READS_QUERY_PORT/,vCRQP,$0);gsub(/SAMPLES_QUERY_PORT/,vSQP,$0);gsub(/COUNTS_RESULT_PORT/,vCRP,$0);gsub(/READS_SAMPLES_RESULT_PORT/,vRSRP,$0);gsub(/INST_DST/,vID,$0);gsub(/INST_BIN_DIR/,vIBD,$0);print $0;}END{}' > ${INST_DST}/SERVER/exit_demo_server.sh

#..........................................................................................................................................................................................................................................#

	##################################################################
		if [[ "${INST_START_AT}" -eq "${INST_STOP_AT}" ]] ; then
			exit 0
		else
			INST_START_AT=$(( INST_START_AT + 1 ))
		fi
	##################################################################

fi
#------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------#
############################################################################################################################################################################################################################################

#==========================================================================================================================================================================================================================================#
