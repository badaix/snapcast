/*
 *     This file is part of snapcast
 *     Copyright (C) 2014-2016  Johannes Pohl
 *
 *     This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package de.badaix.snapcast;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;

import de.badaix.snapcast.control.json.Client;
import de.badaix.snapcast.control.json.Group;
import de.badaix.snapcast.control.json.ServerStatus;


/**
 * A simple {@link Fragment} subclass.
 * Activities that contain this fragment must implement the
 * {@link GroupItem.GroupItemListener} interface
 * to handle interaction events.
 */
public class GroupListFragment extends Fragment {

    private static final String TAG = "GroupList";

    // TODO: Rename parameter arguments, choose names that match
    // the fragment initialization parameters, e.g. ARG_ITEM_NUMBER
    private static final String ARG_PARAM1 = "param1";
    private static final String ARG_PARAM2 = "param2";

    private GroupItem.GroupItemListener groupItemListener;
    private GroupAdapter groupAdapter;
    private ServerStatus serverStatus = null;
    private boolean hideOffline = false;

    public GroupListFragment() {
        // Required empty public constructor
    }

    /**
     * Use this factory method to create a new instance of
     * this fragment using the provided parameters.
     * <p/>
     * //@param param1 Parameter 1.
     *
     * @return A new instance of fragment GroupListFragment.
     * // TODO: Rename and change types and number of parameters
     * public static GroupListFragment newInstance(String param1) {
     * GroupListFragment fragment = new GroupListFragment();
     * Bundle args = new Bundle();
     * args.putString(ARG_PARAM1, param1);
     * fragment.setArguments(args);
     * return fragment;
     * }
     */

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
//            mParam1 = getArguments().getString(ARG_PARAM1);
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        // Inflate the layout for this fragment
        Log.d(TAG, "onCreateView: " + this.toString());
        View view = inflater.inflate(R.layout.fragment_group_list, container, false);
        ListView lvGroup = (ListView) view.findViewById(R.id.lvGroup);
        groupAdapter = new GroupAdapter(getContext(), groupItemListener);
        groupAdapter.setHideOffline(hideOffline);
        groupAdapter.updateServer(serverStatus);
        lvGroup.setAdapter(groupAdapter);
        updateGui();
        return view;
    }


    private void updateGui() {
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        if (context instanceof GroupItem.GroupItemListener) {
            groupItemListener = (GroupItem.GroupItemListener) context;
        } else {
            throw new RuntimeException(context.toString()
                    + " must implement GroupItemListener");
        }
        updateGui();
    }

    @Override
    public void onDetach() {
        super.onDetach();
        groupItemListener = null;
    }

    public void updateServer(final ServerStatus serverStatus) {
        this.serverStatus = serverStatus;
        if (groupAdapter != null)
            groupAdapter.updateServer(serverStatus);
    }

    public void setHideOffline(boolean hide) {
        this.hideOffline = hide;
        if (groupAdapter != null)
            groupAdapter.setHideOffline(hideOffline);
    }

    public class GroupAdapter extends ArrayAdapter<Group> {
        private Context context;
        private GroupItem.GroupItemListener listener;
        private boolean hideOffline = false;
        private ServerStatus serverStatus = new ServerStatus();

        public GroupAdapter(Context context, GroupItem.GroupItemListener listener) {
            super(context, 0);
            this.context = context;
            this.listener = listener;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            Group group = getItem(position);
            final GroupItem groupItem;

            if (convertView != null) {
                groupItem = (GroupItem) convertView;
                groupItem.setGroup(group);
            } else {
                groupItem = new GroupItem(context, serverStatus, group);
            }
            groupItem.setHideOffline(hideOffline);
            groupItem.setListener(listener);
            return groupItem;
        }

        public void updateServer(final ServerStatus serverStatus) {
            if (serverStatus != null) {
                GroupAdapter.this.serverStatus = serverStatus;
                update();
            }
        }


        public void update() {
            getActivity().runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    clear();
// TODO: group
                    for (Group group : GroupAdapter.this.serverStatus.getGroups()) {
//                        add(group);
                        if (group.getClients().isEmpty())
                            continue;

                        int onlineCount = 0;
                        int count = 0;
                        for (Client client : group.getClients()) {
                            if (client == null || client.isDeleted())
                                continue;
                            if (client.isConnected())// && client.getConfig().getStream().equals(GroupListFragment.this.stream.getId()))
                                onlineCount++;
                            count++;
                        }

                        if ((onlineCount > 0) || (!hideOffline && (count > 0)))
                            add(group);
                    }

                    if (getActivity() != null)
                        notifyDataSetChanged();
                }
            });
        }

        public void setHideOffline(boolean hideOffline) {
            if (this.hideOffline == hideOffline)
                return;
            this.hideOffline = hideOffline;
            update();
        }
    }


}
