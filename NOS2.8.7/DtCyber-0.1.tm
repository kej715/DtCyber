#
#  Tcl utility package for interacting with DtCyber.
#
#  Author: William Schaub
#
  namespace eval pkgtemp {
      set modver [file root [file tail [info script]]]
      lassign [split $modver -] ::pkgtemp::ns ::pkgtemp::version
  }
  package provide $::pkgtemp::ns [namespace eval $::pkgtemp::ns {
      namespace export error_condition console dsd mount umount load_job check_job run_job port profile printer_file
      variable version $::pkgtemp::version
      #initialise your module's namespace here (or in the next namespace eval block if desired)
      set version ;#this statement must be the last line in this block (version number is returned to the 'package provide' statement)
  }]
  namespace eval $::pkgtemp::ns {
      #module procs here

set profile ""
set printer_file "LP5xx_C07_E7"
#print error message and return control to user
proc error_condition { cmd } {
    send_user "\n***** Error condition encountered *****\n"
    send_user "Trying to execute $cmd\n"
    send_user "Entering interact mode. Type interp to get an interpreter.\n"
    interact "interp\r" { interpreter } 
}

#wrapper for sending commands to dtcyber console
proc console { cmd } {
    if {$cmd eq "shutdown"} {
       send -- "shutdown\r"
       expect "Shutting down main thread"
       wait 
       exit 0
    }
    send -- "$cmd\r"
    expect {
        "Command not implemented" { error_condition "console $cmd" }
        "Not enough or invalid parameters" { error_condition "console $cmd" }
        "Failed to open" { error_condition "console $cmd" }
        -re "\nOperator> $" {}
    }
    sleep 1
}

#wrapper for sending commands to dsd display via the e console command.
proc dsd { cmd } {
    console "e $cmd"
}

#mount tape on unit if ring is specified it is mounted in write mode
proc mount { unit tape {ring ""} } {
    if {$ring eq ""} {
        send -- "lt $unit,r,$tape\r"
        expect  {
            "Successfully loaded $tape" exp_continue
            "Failed to open" { error_condition "mount $unit $tape" }
            "Not enough or invalid parameters" { error_condition "mount $unit $tape" }
            "Invalid" { error_condition "mount $unit $tape" }
        -re "\nOperator> $" {}
        }
    } else {
        send "lt $unit,w,$tape\r"
        expect {
            "Successfully loaded $tape" exp_continue
            "Failed to open" { error_condition "mount $unit $tape ring" }
            "Not enough or invalid parameters" { error_condition "mount $unit $tape ring" }
            "Invalid" { error_condition "mount $unit $tape ring" }
        -re "\nOperator> $" {}
        }
    }
    sleep 1
}

#unmount tape from unit
proc unmount { unit } {
    send "ut $unit\r"
    expect  {
        "Successfully unloaded" exp_continue
        "not loaded" exp_continue
        "Not enough or invalid parameters" { error_condition "unmount $unit " }
        "Invalid" { error_condition "unmount $unit" }
    -re "\nOperator> $" {}
    }
    sleep 1
}

# load cards on a card reader to start a job
proc load_job { ch eq deck } {
    send -- "lc $ch,$eq,$deck\r"
    expect  {
        "Cards loaded on card reader" { exp_continue }
        "Failed to" { error_condition "load_job $ch $eq" }
        "Not enough or invalid parameters" { error_condition "load_job $ch $eq" }
        "Invalid" { error_condition "load_job $ch $eq" }
        "Input tray full" { error_condition "load_job $ch $eq" }
    -re "\nOperator> $" {}
    }
}

# check printer spawn id for successful job completion
# jobs must produce printer output and a dayfile 
# entry must be shown in the format of * ident complete or * ident failed
# depending on the outcome of the job.
# placing the failed comment after an EXIT. control card
# and placing the complete comment just before in the job is a good idea. 
# Jobs that don't produce output to OUTPUT should contain a DAYFILE. control card.
proc check_job { ident } {
    global printer
    if { $printer eq ""} {
        puts "Something went very wrong"
        exit 1
    }
    expect {
        -i $printer -nocase "\\*\\*\\* $ident FAILED" {
             send_user "$ident failed\n"
             error_condition "job $ident failed"
           }
        -i $printer -nocase "\\*\\*\\* $ident COMPLETE" {
             send_user "$ident complete\n"
           }
    }
}

# load a job on a card reader and wait for it to complete
proc run_job { ch eq deck } {
    set f [open $deck "r"]
    while {1} {
      if {[gets $f line] < 0} {
        break
      }
      if {[regexp "^~" $line]} {
        continue
      }
      regexp "^(\[^,.]*)" $line match jobname
      break
    }
    close $f
    load_job $ch $eq $deck
    send_user "\n$jobname started\n"
    log_user 0
    check_job $jobname
    log_user 1
}


#initialize dtcyber and listen on the printer
#this runs in the global namespace to allow the caller to
#continue to use normal expect commands
proc init {} {
    namespace eval :: {
        set timeout -1
        spawn ./dtcyber $DtCyber::profile
        set dtcyber $spawn_id
        match_max 100000
        expect "Operator> "
        spawn tail -F $DtCyber::printer_file
        set printer $spawn_id
        set spawn_id $dtcyber
        sleep 1
    }
}

# connect to the dtcyber operator console interface on a specified port
#this runs in the global namespace to allow the caller to
#continue to use normal expect commands
proc connect {} {
    namespace eval :: {
        set timeout -1
        spawn nc localhost $DtCyber::port
        set dtcyber $spawn_id
        match_max 100000
        expect "Operator> "
        spawn tail -F $DtCyber::printer_file
        set printer $spawn_id
        set spawn_id $dtcyber
    }
}

}
  namespace delete pkgtemp
