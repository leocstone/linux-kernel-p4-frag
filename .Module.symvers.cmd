cmd_/home/gbolet/finalProj/Module.symvers := sed 's/\.ko$$/\.o/' /home/gbolet/finalProj/modules.order | scripts/mod/modpost    -o /home/gbolet/finalProj/Module.symvers -e -i Module.symvers   -T -
