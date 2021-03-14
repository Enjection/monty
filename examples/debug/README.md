NOTES

    d=~/code/jee/monty/examples/debug
    g=~/Desktop/bpmget
    t=~/code/git/itm-tools/target/debug/

    cd $d
    inv flash debug watch -r mem500.py

    $g/bmp_traceswo >a & f=$!; sleep 10; kill $f; wc a

    $t/itm-decode a | grep -v None | grep -v Some

        StimulusPortPage { page: 0 }
        DataTraceDataValue { cmpn: 0, value: [0], wnr: false }
        StimulusPortPage { page: 0 }
        MalformedPacket { header: 23, len: 2 }

    $t/pcsampl a -e $d/.pio/build/bluepill/firmware.elf

            % FUNCTION
         0.02 *SLEEP*
        25.20 _ZN4PyVM5innerEv
        16.92 _ZNK5monty5ValueeqES0_
         8.81 _ZN5monty7gcCheckEv
         8.36 _ZNK5monty3Set4findENS_5ValueE
         5.81 _ZNK5monty5Value3tagEv
         4.55 _ZN4PyVM6caughtEv
         3.97 _ZN4PyVM3runEv
         3.85 _ZN5monty5gcMaxEv
         3.72 _ZN5monty3Vec9findSpaceEm
         2.82 _ZNK5monty5Value5isQidEv
         2.01 _ZNK5monty5Value5binOpENS_5BinOpES0_
         1.69 _ZN5monty4Dict5ProxyaSENS_5ValueE
         1.64 _ZN4PyVM6fetchOEv.isra.18
         1.49 _ZN5monty7RawIter7stepperEv
         1.32 _ZNK5monty4Dict2atENS_5ValueE
         1.19 _ZN4PyVM6fetchQEv
         1.02 memset
        [...]
