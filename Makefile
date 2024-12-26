.PHONY: hammeriso
hammeriso:iso clean
	sudo mv ISO/out/archlinux*.iso tmp.iso
	sudo rm -rf ISO/out
	sudo chown $(USER) tmp.iso
	sh utilities/generateFinalISO.sh tmp.iso hammeriso.iso loop21
	-ls -lh tmp.iso
	-ls -lh hammeriso.iso
	sudo rm tmp.iso

.PHONY: iso
iso:
	cp -r ARHE ISO/airootfs/root/
	cd ISO && sudo mkarchiso -v ./

.PHONY: clean
clean:
	-sudo rm -rf ISO/work
	-rm -r ISO/airootfs/root/ARHE
