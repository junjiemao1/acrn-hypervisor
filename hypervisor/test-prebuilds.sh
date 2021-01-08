#!/bin/bash

board_dir=../misc/vm_configs/xmls/board-xmls
scenario_dir=../misc/vm_configs/xmls/config-xmls
cached_dir=../misc/vm_configs/scenarios

find ${cached_dir} -name '*.config' | while read f; do
    relpath=$(realpath --relative-to=${cached_dir} ${f})
    IFS="/" read -r -a parts <<< ${relpath}
    board=${parts[1]}
    scenario=${parts[0]}

    make clean > /dev/null && make BOARD=${board} SCENARIO=${scenario} pre_build > /dev/null 2>&1
    # make clean > /dev/null && make BOARD=${board_dir}/${board}.xml SCENARIO=${scenario_dir}/${board}/${scenario}.xml pre_build > /dev/null 2>&1
    # make clean > /dev/null && make BOARD=${board} SCENARIO=${scenario_dir}/${board}/${scenario}.xml pre_build > /dev/null 2>&1
    # make clean > /dev/null && make BOARD=${board_dir}/${board}.xml SCENARIO=${scenario} pre_build > /dev/null 2>&1
    # make clean > /dev/null && make BOARD_FILE=${board_dir}/${board}.xml SCENARIO_FILE=${scenario_dir}/${board}/${scenario}.xml pre_build > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "${board} + ${scenario}: BUILD ERROR"
    else
        cat build/configs/*.mk | grep -v include | grep -v BOARD_FILE | grep -v SCENARIO | grep -v SERIAL_PCI_BDF | grep -v "=n$" | grep -v "#" | sort > generated
        cat ${f} | grep -v UEFI_OS_LOADER | grep -v SERIAL_PCI_BDF | grep -v "#" | grep -v "^$" | grep -v "=n$" | sed 's/"//g' | sort > cached
        if diff generated cached && [ -d build/configs/boards/${board} ] && [ -d build/configs/scenarios/${scenario} ]; then
            echo "${board} + ${scenario}: PASS"
        else
            echo "${board} + ${scenario}: FAILED"
        fi
        rm generated cached
    fi
done
