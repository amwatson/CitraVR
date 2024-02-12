package org.citra.citra_emu.vr.ui

import VrNoticePageFragment
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity

import androidx.viewpager2.adapter.FragmentStateAdapter

class VrNoticePagerAdapter(fa: FragmentActivity?) : FragmentStateAdapter(fa!!) {
    override fun createFragment(position: Int): Fragment {
        return VrNoticePageFragment.newInstance(position)
    }

    override fun getItemCount(): Int {
        return 2 // The number of pages
    }
}