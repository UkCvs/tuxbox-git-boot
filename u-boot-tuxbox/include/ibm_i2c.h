#ifndef _IBM_I2C_H
#define _IBM_I2C_H

#if defined(CONFIG_STB04xxx)
#define	   I2C_REGISTERS_BASE_ADDRESS 0x400b0000	// use second i2c device master
#elif defined(CONFIG_STBx25xx)
#define	   I2C_REGISTERS_BASE_ADDRESS 0x40030000	// use first i2c device master
#elif defined(CONFIG_STB034xx)
#define	   I2C_REGISTERS_BASE_ADDRESS 0x400b0000	// use second i2c device master
#else
#error "no cpu is specified..."
#endif

#define    IIC_MDBUF	(I2C_REGISTERS_BASE_ADDRESS+IICMDBUF)
#define    IIC_SDBUF	(I2C_REGISTERS_BASE_ADDRESS+IICSDBUF)
#define    IIC_LMADR	(I2C_REGISTERS_BASE_ADDRESS+IICLMADR)
#define    IIC_HMADR	(I2C_REGISTERS_BASE_ADDRESS+IICHMADR)
#define    IIC_CNTL	(I2C_REGISTERS_BASE_ADDRESS+IICCNTL)
#define    IIC_MDCNTL	(I2C_REGISTERS_BASE_ADDRESS+IICMDCNTL)
#define    IIC_STS	(I2C_REGISTERS_BASE_ADDRESS+IICSTS)
#define    IIC_EXTSTS	(I2C_REGISTERS_BASE_ADDRESS+IICEXTSTS)
#define    IIC_LSADR	(I2C_REGISTERS_BASE_ADDRESS+IICLSADR)
#define    IIC_HSADR	(I2C_REGISTERS_BASE_ADDRESS+IICHSADR)
#define    IIC_CLKDIV	(I2C_REGISTERS_BASE_ADDRESS+IICCLKDIV)
#define    IIC_INTRMSK	(I2C_REGISTERS_BASE_ADDRESS+IICINTRMSK)
#define    IIC_XFRCNT	(I2C_REGISTERS_BASE_ADDRESS+IICXFRCNT)
#define    IIC_XTCNTLSS	(I2C_REGISTERS_BASE_ADDRESS+IICXTCNTLSS)
#define    IIC_DIRECTCNTL (I2C_REGISTERS_BASE_ADDRESS+IICDIRECTCNTL)

/* MDCNTL Register Bit definition */
#define    IIC_MDCNTL_HSCL 0x01
#define    IIC_MDCNTL_EUBS 0x02
#define    IIC_MDCNTL_EINT 0x04
#define    IIC_MDCNTL_ESM  0x08
#define    IIC_MDCNTL_FSM  0x10
#define    IIC_MDCNTL_EGC  0x20
#define    IIC_MDCNTL_FMDB 0x40
#define    IIC_MDCNTL_FSDB 0x80

/* CNTL Register Bit definition */
#define    IIC_CNTL_PT     0x01
#define    IIC_CNTL_READ   0x02
#define    IIC_CNTL_CHT    0x04
#define    IIC_CNTL_RPST   0x08
/* bit 2/3 for Transfer count*/
#define    IIC_CNTL_AMD    0x40
#define    IIC_CNTL_HMT    0x80

/* STS Register Bit definition */
#define    IIC_STS_PT	   0X01
#define    IIC_STS_IRQA    0x02
#define    IIC_STS_ERR	   0X04
#define    IIC_STS_SCMP    0x08
#define    IIC_STS_MDBF    0x10
#define    IIC_STS_MDBS    0X20
#define    IIC_STS_SLPR    0x40
#define    IIC_STS_SSS     0x80

/* EXTSTS Register Bit definition */
#define    IIC_EXTSTS_XFRA 0X01
#define    IIC_EXTSTS_ICT  0X02
#define    IIC_EXTSTS_LA   0X04

/* XTCNTLSS Register Bit definition */
#define    IIC_XTCNTLSS_SRST  0x01
#define    IIC_XTCNTLSS_EPI   0x02
#define    IIC_XTCNTLSS_SDBF  0x04
#define    IIC_XTCNTLSS_SBDD  0x08
#define    IIC_XTCNTLSS_SWS   0x10
#define    IIC_XTCNTLSS_SWC   0x20
#define    IIC_XTCNTLSS_SRS   0x40
#define    IIC_XTCNTLSS_SRC   0x80
#endif
