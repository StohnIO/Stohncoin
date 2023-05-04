// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2021 The Stohn Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include "logging.h"

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{

  // #HARDFORK2023 Update
  int64_t difficultyAdjustmentInterval = params.DifficultyAdjustmentInterval();
  int64_t nTargetTimespan = params.nPowTargetTimespan;

    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval

    // Check if we have reached the hard fork block height
    // #HARDFORK2023 Update

    if (pindexLast->nHeight >= params.HardFork_Height) {
        difficultyAdjustmentInterval = params.DifficultyAdjustmentInterval_Fork();
        nTargetTimespan = params.nPowTargetTimespan_Fork;
    } else {
        difficultyAdjustmentInterval = params.DifficultyAdjustmentInterval();
        nTargetTimespan = params.nPowTargetTimespan;
    }

    LogPrintf("Difficulty Adjustment Interval: %d\n", difficultyAdjustmentInterval);

    // #HARDFORK2023 Update
    if ((pindexLast->nHeight+1) % difficultyAdjustmentInterval != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;

                //while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                // #HARDFORK2023
                while (pindex->pprev && pindex->nHeight % difficultyAdjustmentInterval != 0 && pindex->nBits == nProofOfWorkLimit){
                    pindex = pindex->pprev;

                    // #HARDFORK2023 Log - added {}
                    LogPrintf("Difficulty target for block at height %d is %08x\n", pindex->nHeight, pindex->nBits);

                }

                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    // Stohn: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz

    int blockstogoback = difficultyAdjustmentInterval - 1;
    if ((pindexLast->nHeight+1) != difficultyAdjustmentInterval)
        blockstogoback = difficultyAdjustmentInterval;


    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;

    assert(pindexFirst);

    // #HARDFORK2023 Update
    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params, nTargetTimespan);

}

// #HARDFORK2023 Update
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params, int64_t nTargetTimespan)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // ...

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    LogPrintf("nActualTimespan: %lld\n", nActualTimespan);

      // #HARDFORK2023 Update
      if (nActualTimespan < nTargetTimespan/4)
          nActualTimespan = nTargetTimespan/4;
      if (nActualTimespan > nTargetTimespan*4)
          nActualTimespan = nTargetTimespan*4;

      LogPrintf("nActualTimespan (after limits): %lld\n", nActualTimespan);

    // Retarget
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    // Stohn: intermediate uint256 can overflow by 1 bit
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
    if (fShift)
        bnNew >>= 1;

    // #HARDFORK2023 Update
    bnNew *= nActualTimespan;
    bnNew /= nTargetTimespan;

    if (fShift)
        bnNew <<= 1;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    LogPrintf("Old target: %s\n", bnOld.ToString());
    LogPrintf("New target: %s\n", bnNew.ToString());


    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
