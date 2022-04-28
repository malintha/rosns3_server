# Deployment parameters
N_BACKBONES=5
ISDYNAMIC=0
MAX_ITERATIONS=1
TIME="400"

# Friis loss model parameters
path_loss_exponent=2.4
reference_loss=46;
transmission_power=16.0206;
fading_mean=0;
fading_var=32;

echo "Max iterations: "$MAX_ITERATIONS

for i in `seq 1 $MAX_ITERATIONS`
do

     # move to ns3 dir
    cd ~/ns3/ns-allinone-3.30/ns-3.30
    CMD_RND="export NS_GLOBAL_VALUE="RngRun=$i""
    $CMD_RND
    CMD_WAF="./waf --run 'rosns3-example --exp=$path_loss_exponent --ref_loss=$reference_loss --tx_power=$transmission_power --mean=$fading_mean --var=$fading_var'"
    log="waf_log.log"
    eval $CMD_WAF > "$log" 2>&1 &
    waf_pid=$!

    sleep 1
    cd ~/cov_ws
    source ./devel/setup.bash

    #run simu.launch
    CMD_SIMU="roslaunch multi_uav_simulator simu_$N_BACKBONES.launch is_dynamic:=$ISDYNAMIC"
    echo $CMD_SIMU
    $CMD_SIMU&
    simu_pid=$!

    # echo $pid

    # run mrf_dynamics
    CMD_MRF="roslaunch mrf_dynamics mrf_$N_BACKBONES.launch is_dynamic:=$ISDYNAMIC --wait"
    echo $CMD_MRF
    # log="mrf_log.log"
    eval $CMD_MRF > "$log" 2>&1 &
    mrf_pid=$!
    sleep 1


    # move back to ns3 dir
    cd ~/ns3/ns-allinone-3.30/ns-3.30

    while sleep 1
    do
        if fgrep "$TIME" "$log"
        then
            kill $waf_pid
            killall "ns3.30-rosns3-example-debug"
            kill $simu_pid
            kill $mrf_pid
            killall "mrf_dynamics"
	    killall "roslaunch"
            # exit 0
            break
        fi
    done
sleep 2
done


#cd ~/ns3/ns-allinone-3.30/ns-3.30

#POSTFIX="_dynamic.txt"
#if [ $ISDYNAMIC -eq 0 ] ;
#then
#    POSTFIX="_static.txt"
#fi
#FILE_NAME="$N_BACKBONES$POSTFIX"
#echo $FILE_NAME
#cp $FILE_NAME ~/Desktop/
#rm $FILE_NAME
