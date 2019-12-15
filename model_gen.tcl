#!/usr/bin/tclsh

# Copyright 2019 Alain Dargelas
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set model_file $argv

# proc copied from: https://wiki.tcl-lang.org/page/pdict%3A+Pretty+print+a+dict
proc pdict { d {i 0} {p "  "} {s " -> "} } {
    global $d
    set fRepExist [expr {0 < [llength\
            [info commands tcl::unsupported::representation]]}]
    if { (![string is list $d] || [llength $d] == 1)
            && [uplevel 1 [list info exists $d]] } {
        set dictName $d
        unset d
        upvar 1 $dictName d
        puts "dict $dictName"
    }
    if { ! [string is list $d] || [llength $d] % 2 != 0 } {
        return -code error  "error: pdict - argument is not a dict"
    }
    set prefix [string repeat $p $i]
    set max 0
    foreach key [dict keys $d] {
        if { [string length $key] > $max } {
            set max [string length $key]
        }
    }
    dict for {key val} ${d} {
        puts -nonewline "${prefix}[format "%-${max}s" $key]$s"
        if {    $fRepExist && [string match "value is a dict*"\
                    [tcl::unsupported::representation $val]]
                || ! $fRepExist && [string is list $val]
                    && [llength $val] % 2 == 0 } {
            puts ""
            pdict $val [expr {$i+1}] $p $s
        } else {
            puts "'${val}'"
        }
    }
    return
}

proc parse_model { file } {
    set models {}
    set fid [open $file]
    set content [read $fid]
    close $fid

    set lines [split $content "\n"]
    set OBJ(curr) ""
    set INDENT(curr) 0
    set modelId 0
    set obj_name ""
    set obj_type ""
    foreach line $lines {
	set spaces ""
	regexp {^([ ]*)} $line tmp spaces
	set indent [string length $spaces]
	if {$indent <= $INDENT(curr)} {
	}
	set INDENT(curr) $indent
	
	if [regexp {\- obj_def: (.*)} $line tmp name] {
	    global obj_def$modelId
	    set obj_def$modelId [dict create "name" $name "type" obj_def "properties" {} "class_ref" {} "obj_ref" {}]	   
	    lappend models obj_def$modelId 
	    set OBJ(curr) obj_def$modelId
	    incr modelId
	}
	if [regexp {property: (.*)} $line tmp name] {
	    dict set $OBJ(curr) "properties" $name {}
	    set obj_name $name
	    set obj_type "properties"
	}
	if [regexp {class_ref: (.*)} $line tmp name] {
	    dict set $OBJ(curr) "class_ref" $name {}
	    set obj_name $name
	    set obj_type "class_ref"
	}
	if [regexp {obj_ref: (.*)} $line tmp name] {
	    dict set $OBJ(curr) "obj_ref" $name {}
	    set obj_name $name
	    set obj_type "obj_ref"
	}
	if [regexp {class: (.*)} $line tmp name] {
	    dict set $OBJ(curr) "class" $name {}
	    set obj_name $name
	    set obj_type "class"
	}
	if [regexp {type: (.*)} $line tmp type] {
	    dict set $OBJ(curr) $obj_type $obj_name type $type
	}
	if [regexp {vpi: (.*)} $line tmp vpiType] {
	    dict set $OBJ(curr) $obj_type $obj_name vpi $vpiType
	}
	if [regexp {card: (.*)} $line tmp card] {
	    dict set $OBJ(curr) $obj_type $obj_name card $card
	}
	if [regexp {name: (.*)} $line tmp name] {
	    dict set $OBJ(curr) $obj_type $obj_name name $name
	}
    }
    return $models
}

proc printMethods { type vpi card } {
    if {$card == "1"} {
	append methods "\n    $type get_${vpi}() { return m_$vpi; }\n"
	append methods "\n    void set_${vpi}($type data) { m_$vpi = data; }\n"
    } elseif {$card == "any"} {
	append methods "\n    const VectorOf${type}Ref get_${vpi}() { return m_$vpi; }\n"
	append methods "\n    void set_${vpi}(VectorOf${type}Ref data) { m_$vpi = data; }\n"	
    }
    return $methods
}

proc printMembers { type vpi card } {
    if {$card == "1"} {
	append members "\n    $type m_$vpi;\n"
    } elseif {$card == "any"} {
	append members "\n    VectorOf${type} m_$vpi;\n"	
    }
}

proc printTypeDefs { containerId type card } {
    global CONTAINER
    if {$card == "any"} {
	if ![info exist CONTAINER($type)] {
	    set CONTAINER($type) 1
	    puts $containerId "  class ${type};"
	    puts $containerId "  typedef std::vector<${type}*> VectorOf${type};"
	    puts $containerId "  typedef std::vector<${type}*>& VectorOf${type}Ref;"
	}
    }
}

proc generate_headers { models } {
    puts "=========="
    exec sh -c "mkdir -p headers"
    exec sh -c "mkdir -p src"
    set fid [open "templates/class_header.h"]
    set template_content [read $fid]
    close $fid
    set mainId [open "src/main.cpp" "w"]
    puts $mainId "#include <string>"
    puts $mainId "#include <vector>"
    puts $mainId "#include \"include/vpi_user.h\""
    puts $mainId "#include \"headers/containers.h\""
    set containerId [open "headers/containers.h" "w"]
    
    foreach model $models {
	global $model
	puts "** $model **"
	set data [subst $$model] 
	set classname [dict get $data name]
	set template $template_content

	puts "Generating headers/$classname.h"
	puts $mainId "#include \"headers/$classname.h\""
	set oid [open "headers/$classname.h" "w"]
	regsub -all {<CLASSNAME>} $template $classname template
	regsub -all {<UPPER_CLASSNAME>} $template [string toupper $classname] template
	set methods ""
	set members ""
	dict for {key val} $data {
	   # puts "$key $val"
	    if {$key == "properties"} {
		dict for {prop conf} $val {
		    set name [dict get $conf name]
		    set vpi  [dict get $conf vpi]
		    set type [dict get $conf type]
		    set card [dict get $conf card]
		    printTypeDefs $containerId $type $card
		    append methods [printMethods $type $vpi $card] 
		    append members [printMembers $type $vpi $card]
		}
	    }
	    if {$key == "class"} {
		dict for {iter content} $val {
		    puts "$iter $content"
		    set name $iter
		    set vpi  [dict get $content vpi]
		    set type [dict get $content type]
		    set card [dict get $content card]
		    printTypeDefs $containerId $type $card
		    append methods [printMethods $type $name $card] 
		    append members [printMembers $type $name $card]
		}
	    }
	}
	regsub -all {<METHODS>} $template $methods template
	regsub -all {<MEMBERS>} $template $members template
	
	puts $oid $template
	close $oid
	
    }
    puts $mainId "int main () { };"
    close $mainId
    close $containerId

    
}

proc debug_models { models } {
    # Model printout
    foreach model $models {
	puts "=========="
	global $model
	pdict $model
    }
}


set models [parse_model $model_file]

debug_models $models

generate_headers $models




