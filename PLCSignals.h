#ifndef PLCSIGNALS_H
#define PLCSIGNALS_H

#define FA_1_NUM 0                        // пожарный извещатель 1 номер бита
#define FA_2_NUM 1                        // пожарный извещатель 2 номер бита
#define SPDP_NUM 2                        // система поддержания давления в шахте пассажирского лифта номер бита
#define CPDG_NUM 3                        // система поддержания давления в шахте грузового лифта номер бита
#define KP_1_NUM 4                        // клапан пожарный, перекрывающий вентиляцию 1 номер бита
#define KP_2_NUM 5                        // клапан пожарный, перекрывающий вентиляцию 2 номер бита
#define VP_NUM 6                          // вентилятор притяжной номер бита
#define VV_NUM 7                          // вентилятор вытяжной номер бита
#define BDP_NUM 8                         // блок диспетчеризации пассажирского лифта номер бита
#define BDG_NUM 9                         // блок диспетчеризации грузового лифта номер бита
#define NVO_NUM 10                        // насос водооткачивающий номер бита

struct PLCSignals
{
	short int waterLevel; // уровень воды в приямке в миллиметрах максимум 300 при 150 вкл насос
    short int gasLevel; // давление газа в системе пожаротушения в процентах
	short int temperLevel; // уровень температуры в машинном помещении
	unsigned short int booleanValues; // Булевы параметры
};

bool getBitFromPlcSgn(unsigned short bitarr, int numofbit)
{
	if(numofbit > 15 or numofbit < 0) return false;
	bitarr = bitarr << (15-numofbit);
	bitarr = bitarr >> 15;
	return bitarr > 0;
}

bool setBitInPlcSgn(unsigned short* bitarr, int numofbit)
{
	if(numofbit > 15 or numofbit < 0) return false;
	unsigned short ads = 1;
	ads = ads << (numofbit);
	*bitarr = *bitarr | ads;
	return true;
}

bool resetBitInPlcSgn(unsigned short* bitarr, int numofbit)
{
	if(numofbit > 15 or numofbit < 0) return false;
	unsigned short ads = 1;
	ads = ads << (numofbit);
	*bitarr = *bitarr | ads;
	*bitarr = *bitarr ^ ads;
	return true;
}

#endif //PLCSIGNALS_H