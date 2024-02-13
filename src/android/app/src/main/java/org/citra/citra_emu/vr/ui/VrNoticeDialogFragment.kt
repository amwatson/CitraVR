import org.citra.citra_emu.R
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.fragment.app.Fragment
class VrNoticePageFragment : Fragment() {
    private var position = 0
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (arguments != null) {
            position = requireArguments().getInt(ARG_POSITION)
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val rootView: View = inflater.inflate(R.layout.vr_notice_dialog_fragment, container, false)
        val body = rootView.findViewById<TextView>(R.id.notice_body)
         body.text = if (position == 0) {resources.getString(R.string.vr_notice_description)} else {resources.getString(R.string.vr_notice_description2)}
        return rootView
    }

    companion object {
        private const val ARG_POSITION = "position"
        fun newInstance(position: Int): VrNoticePageFragment {
            val fragment = VrNoticePageFragment()
            val args = Bundle()
            args.putInt(ARG_POSITION, position)
            fragment.arguments = args
            return fragment
        }
    }
}
