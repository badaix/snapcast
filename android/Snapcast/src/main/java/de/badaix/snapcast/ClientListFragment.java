package de.badaix.snapcast;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import de.badaix.snapcast.control.ClientInfo;
import de.badaix.snapcast.control.ServerInfo;
import de.badaix.snapcast.control.Stream;


/**
 * A simple {@link Fragment} subclass.
 * Activities that contain this fragment must implement the
 * {@link ClientListFragment.OnFragmentInteractionListener} interface
 * to handle interaction events.
 * Use the {@link ClientListFragment#newInstance} factory method to
 * create an instance of this fragment.
 */
public class ClientListFragment extends Fragment {

    private static final String TAG = "ClientList";

    // TODO: Rename parameter arguments, choose names that match
    // the fragment initialization parameters, e.g. ARG_ITEM_NUMBER
    private static final String ARG_PARAM1 = "param1";
    private static final String ARG_PARAM2 = "param2";

    // TODO: Rename and change types of parameters
    private Stream stream;
    private String mParam1;

    private OnFragmentInteractionListener mListener;
    private ClientInfoItem.ClientInfoItemListener clientInfoItemListener;
    private ClientInfoAdapter clientInfoAdapter;
    private ServerInfo serverInfo = null;
    private boolean hideOffline = false;

    public ClientListFragment() {
        // Required empty public constructor
    }

    /**
     * Use this factory method to create a new instance of
     * this fragment using the provided parameters.
     *
     * @param param1 Parameter 1.
     * @return A new instance of fragment ClientListFragment.
     */
    // TODO: Rename and change types and number of parameters
    public static ClientListFragment newInstance(String param1) {
        ClientListFragment fragment = new ClientListFragment();
        Bundle args = new Bundle();
        args.putString(ARG_PARAM1, param1);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
            mParam1 = getArguments().getString(ARG_PARAM1);
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        // Inflate the layout for this fragment
        View view = inflater.inflate(R.layout.fragment_client_list, container, false);
        ListView lvClient = (ListView) view.findViewById(R.id.lvClient);
        clientInfoAdapter = new ClientInfoAdapter(getContext(), clientInfoItemListener);
        clientInfoAdapter.setHideOffline(hideOffline);
        clientInfoAdapter.updateServer(serverInfo);
        lvClient.setAdapter(clientInfoAdapter);
        return view;
    }

    // TODO: Rename method, update argument and hook method into UI event
    public void onButtonPressed(Uri uri) {
        if (mListener != null) {
            mListener.onFragmentInteraction(uri);
        }
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        if (context instanceof OnFragmentInteractionListener) {
            mListener = (OnFragmentInteractionListener) context;
        } else {
            throw new RuntimeException(context.toString()
                    + " must implement OnFragmentInteractionListener");
        }
        clientInfoItemListener = (ClientInfoItem.ClientInfoItemListener) context;
    }

    @Override
    public void onDetach() {
        super.onDetach();
        mListener = null;
    }

    public void updateServer(ServerInfo serverInfo) {
        this.serverInfo = serverInfo;
        if (clientInfoAdapter != null)
            clientInfoAdapter.updateServer(this.serverInfo);
    }

    public void setHideOffline(boolean hide) {
        this.hideOffline = hide;
        if (clientInfoAdapter != null)
            clientInfoAdapter.setHideOffline(hideOffline);
    }

    public String getName() {
        return stream.getQuery().get("name");
    }

    public void setStream(Stream stream) {
        this.stream = stream;
    }


    /**
     * This interface must be implemented by activities that contain this
     * fragment to allow an interaction in this fragment to be communicated
     * to the activity and potentially other fragments contained in that
     * activity.
     * <p/>
     * See the Android Training lesson <a href=
     * "http://developer.android.com/training/basics/fragments/communicating.html"
     * >Communicating with Other Fragments</a> for more information.
     */
    public interface OnFragmentInteractionListener {
        // TODO: Update argument type and name
        void onFragmentInteraction(Uri uri);
    }


    public class ClientInfoAdapter extends ArrayAdapter<ClientInfo> {
        private Context context;
        private ClientInfoItem.ClientInfoItemListener listener;
        private boolean hideOffline = false;
        private ServerInfo serverInfo = new ServerInfo();

        public ClientInfoAdapter(Context context, ClientInfoItem.ClientInfoItemListener listener) {
            super(context, 0);
            this.context = context;
            this.listener = listener;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            ClientInfo clientInfo = getItem(position);
            final ClientInfoItem clientInfoItem;

            if (convertView != null) {
                clientInfoItem = (ClientInfoItem) convertView;
                clientInfoItem.setClientInfo(clientInfo);
            } else {
                clientInfoItem = new ClientInfoItem(context, clientInfo);
            }
            clientInfoItem.setListener(listener);
            return clientInfoItem;
        }

        public void updateServer(final ServerInfo serverInfo) {
            if (serverInfo != null) {
                ClientInfoAdapter.this.serverInfo = serverInfo;
                update();
            }
        }


        public void update() {

            ClientListFragment.this.getActivity().runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    clear();
                    for (ClientInfo clientInfo : ClientInfoAdapter.this.serverInfo.getClientInfos()) {
                        if ((clientInfo != null) && (!hideOffline || clientInfo.isConnected()) && !clientInfo.isDeleted() && clientInfo.getStream().equals(ClientListFragment.this.stream.getId()))
                            add(clientInfo);
                    }
                    notifyDataSetChanged();
                }
            });
        }

        public boolean isHideOffline() {
            return hideOffline;
        }

        public void setHideOffline(boolean hideOffline) {
            if (this.hideOffline == hideOffline)
                return;
            this.hideOffline = hideOffline;
            update();
        }
    }


}
